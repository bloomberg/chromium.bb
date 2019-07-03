/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import * as testUtils from './cache-request-base.js';
import idb from 'idb';
import {KeyValueCacheRequest} from './key-value-cache-request.js';
import {assert} from 'chai';

class MockFetchEvent {
  constructor(url = 'http://example.com/path') {
    this.request = new Request(url);
  }

  waitUntil() {
  }
}

class TestCacheRequest extends KeyValueCacheRequest {
  constructor(key) {
    super(new MockFetchEvent());
    this.databaseKeyPromise_ = Promise.resolve(key);
  }
}

async function clearData() {
  const db = await idb.open('keyvalue', 1, db => db.createObjectStore('data'));
  const transaction = db.transaction(['data'], 'readwrite');
  const dataStore = transaction.objectStore('data');
  await dataStore.clear();
}

suite('KeyValueCacheRequest', function() {
  let originalFetch;
  setup(() => {
    originalFetch = window.fetch;
  });
  teardown(() => {
    window.fetch = originalFetch;
  });

  test('miss', async() => {
    await clearData();
    window.fetch = async() => testUtils.jsonResponse('miss value');
    const cacheRequest = new TestCacheRequest('miss key');
    const response = await cacheRequest.responsePromise;
    assert.strictEqual('miss value', response);

    await testUtils.flushWriterForTest();
    const db = await idb.open('keyvalue', 1, db => {
      throw new Error('databaseUpgrade was unexpectedly called');
    });
    const transaction = db.transaction(['data'], 'readonly');
    const dataStore = transaction.objectStore('data');
    const entry = await dataStore.get('miss key');
    assert.strictEqual('miss value', entry.value);
  });

  test('hit', async() => {
    await clearData();
    window.fetch = async() => {
      throw new Error('fetch was unexpectedly called');
    };
    const db = await idb.open('keyvalue', 1, db => {
      db.createObjectStore('data');
    });
    const transaction = db.transaction(['data'], 'readwrite');
    const dataStore = transaction.objectStore('data');
    const expiration = new Date(new Date().getTime() + 1000);
    dataStore.put({value: 'hit value', expiration}, 'hit key');
    await transaction.complete;

    const cacheRequest = new TestCacheRequest('hit key');
    const response = await cacheRequest.responsePromise;
    assert.strictEqual('hit value', response);
  });

  test('in progress', async() => {
    testUtils.clearInProgressForTest();
    await clearData();
    // Start two requests at the same time. Only one of them should call
    // fetch().
    window.fetch = async() => {
      window.fetch = async() => {
        throw new Error('fetch was unexpectedly called');
      };
      return testUtils.jsonResponse('in progress value');
    };
    const cacheRequestA = new TestCacheRequest('in progress key');
    const cacheRequestB = new TestCacheRequest('in progress key');
    const responseA = await cacheRequestA.responsePromise;
    const responseB = await cacheRequestB.responsePromise;
    assert.strictEqual('in progress value', responseA);
    assert.strictEqual('in progress value', responseB);
  });
});
