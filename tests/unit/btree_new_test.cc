/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "common.h"
#include "bnode.h"


void bnode_basic_test()
{
    TEST_INIT();

    Bnode *bnode = new Bnode();
    BnodeResult ret;
    size_t i;
    size_t n = 100;
    char keybuf[64], valuebuf[64];

    // add test
    for (i=0; i<n; ++i) {
        sprintf(keybuf, "k%07d\n", (int)i);
        sprintf(valuebuf, "v%07d\n", (int)i*10);
        ret = bnode->addKv(keybuf, 8, valuebuf, 8, nullptr, true);
        TEST_CHK(ret == BnodeResult::SUCCESS);
    }
    TEST_CHK(bnode->getNentry() == n);

    // meta
    char metabuf[64];
    sprintf(metabuf, "meta_data");
    bnode->setMeta(metabuf, 9, false);

    // find test
    size_t valuelen_out;
    void* value_out;
    Bnode *bnode_out;
    for (i=0; i<n; ++i) {
        sprintf(keybuf, "k%07d\n", (int)i);
        sprintf(valuebuf, "v%07d\n", (int)i*10);
        ret = bnode->findKv(keybuf, 8, value_out, valuelen_out, bnode_out);
        TEST_CHK(ret == BnodeResult::SUCCESS);
        TEST_CMP(value_out, valuebuf, valuelen_out);
    }

    // export/import test
    char temp_buf[3000];
    memset(temp_buf, 'x', 3000);
    bnode->exportRaw((void*)temp_buf);

    // out-of-bound check
    size_t node_size = bnode->getNodeSize();
    TEST_CHK(temp_buf[node_size] == 'x');

    // read node size
    size_t node_size_from_buffer = Bnode::readNodeSize(temp_buf);
    TEST_CHK(node_size_from_buffer == node_size);

    // import check
    Bnode *bnode_copy = new Bnode();
    bnode_copy->importRaw((void*)temp_buf, true);
    for (i=0; i<n; ++i) {
        sprintf(keybuf, "k%07d\n", (int)i);
        sprintf(valuebuf, "v%07d\n", (int)i*10);
        ret = bnode_copy->findKv(keybuf, 8, value_out, valuelen_out, bnode_out);
        TEST_CHK(ret == BnodeResult::SUCCESS);
        TEST_CMP(value_out, valuebuf, valuelen_out);
    }

    TEST_CMP( bnode_copy->getMeta(),
              bnode->getMeta(),
              bnode_copy->getMetaSize() );

    // meta data update
    sprintf(metabuf, "new_meta_data");
    bnode->setMeta(metabuf, 13, false);
    bnode_copy->setMeta(metabuf, 13, false);

    // remove test
    for (i=0; i<n; ++i) {
        sprintf(keybuf, "k%07d\n", (int)i);
        ret = bnode->removeKv(keybuf, 8);
        TEST_CHK(ret == BnodeResult::SUCCESS);
    }
    TEST_CHK(bnode->getNentry() == 0);

    // remove bnode_copy (existing memory mode)
    for (i=0; i<n; ++i) {
        sprintf(keybuf, "k%07d\n", (int)i);
        ret = bnode_copy->removeKv(keybuf, 8);
        TEST_CHK(ret == BnodeResult::SUCCESS);
    }
    TEST_CHK(bnode_copy->getNentry() == 0);

    delete bnode;
    delete bnode_copy;

    TEST_RESULT("bnode basic test");
}

void bnode_iterator_test()
{
    TEST_INIT();

    Bnode *bnode = new Bnode();
    BnodeIterator *bit;
    BnodeResult ret;
    BnodeIteratorResult bit_ret = BnodeIteratorResult::SUCCESS;
    size_t i;
    size_t n = 100;
    char keybuf[64], valuebuf[64];

    // add test
    for (i=0; i<n; i++) {
        sprintf(keybuf, "k%07d\n", (int)i*10);
        sprintf(valuebuf, "v%07d\n", (int)i*100);
        ret = bnode->addKv(keybuf, 8, valuebuf, 8, nullptr, true);
        TEST_CHK(ret == BnodeResult::SUCCESS);
    }
    TEST_CHK(bnode->getNentry() == n);

    BtreeKv *kvp_out;

    // forward iteration
    bit = new BnodeIterator(bnode);
    i = 0;
    do {
        kvp_out = bit->getKv();
        if (!kvp_out) {
            break;
        }

        sprintf(keybuf, "k%07d\n", (int)i*10);
        sprintf(valuebuf, "v%07d\n", (int)i*100);

        TEST_CMP(keybuf, kvp_out->key, kvp_out->keylen);
        TEST_CMP(valuebuf, kvp_out->value, kvp_out->valuelen);
        i++;

        bit_ret = bit->next();
    } while (bit_ret == BnodeIteratorResult::SUCCESS);
    TEST_CHK(i == n);
    delete bit;

    // backward iteration
    bit = new BnodeIterator(bnode);
    bit_ret = bit->end();
    TEST_CHK(bit_ret == BnodeIteratorResult::SUCCESS);

    i = n;
    do {
        kvp_out = bit->getKv();
        if (!kvp_out) {
            break;
        }

        i--;
        sprintf(keybuf, "k%07d\n", (int)i*10);
        sprintf(valuebuf, "v%07d\n", (int)i*100);

        TEST_CMP(keybuf, kvp_out->key, kvp_out->keylen);
        TEST_CMP(valuebuf, kvp_out->value, kvp_out->valuelen);

        bit_ret = bit->prev();
    } while (bit_ret == BnodeIteratorResult::SUCCESS);
    TEST_CHK(i == 0);
    delete bit;

    // assigning start_key (seekGreater)
    i = n/2;
    sprintf(keybuf, "k%07d\n", (int)i*10 + 5);
    bit = new BnodeIterator(bnode, keybuf, 8);
    i++;

    do {
        kvp_out = bit->getKv();
        if (!kvp_out) {
            break;
        }

        sprintf(keybuf, "k%07d\n", (int)i*10);
        sprintf(valuebuf, "v%07d\n", (int)i*100);
        i++;

        TEST_CMP(keybuf, kvp_out->key, kvp_out->keylen);
        TEST_CMP(valuebuf, kvp_out->value, kvp_out->valuelen);

        bit_ret = bit->next();
    } while (bit_ret == BnodeIteratorResult::SUCCESS);
    TEST_CHK(i == n);
    delete bit;

    // seekSmaller
    bit = new BnodeIterator(bnode);

    i = n/2;
    sprintf(keybuf, "k%07d\n", (int)i*10 - 5);
    bit->seekSmallerOrEqual(keybuf, 8);
    i--;

    do {
        kvp_out = bit->getKv();
        if (!kvp_out) {
            break;
        }

        sprintf(keybuf, "k%07d\n", (int)i*10);
        sprintf(valuebuf, "v%07d\n", (int)i*100);
        i++;

        TEST_CMP(keybuf, kvp_out->key, kvp_out->keylen);
        TEST_CMP(valuebuf, kvp_out->value, kvp_out->valuelen);

        bit_ret = bit->next();
    } while (bit_ret == BnodeIteratorResult::SUCCESS);
    TEST_CHK(i == n);
    delete bit;

    delete bnode;

    TEST_RESULT("bnode iterator test");
}

int main()
{
    bnode_basic_test();
    bnode_iterator_test();
    return 0;
}


