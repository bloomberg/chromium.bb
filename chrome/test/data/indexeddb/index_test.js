// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onCursor()
{
  var cursor = event.result;
  if (cursor === null) {
    debug('Reached end of object cursor.');
    if (!gotObjectThroughCursor) {
      fail('Did not get object through cursor.');
      return;
    }
    done();
    return;
  }

  debug('Got object through cursor.');
  shouldBe('event.result.key', '55');
  shouldBe('event.result.value.aValue', '"foo"');
  gotObjectThroughCursor = true;

  cursor.continue();
}

function onKeyCursor()
{
  var cursor = event.result;
  if (cursor === null) {
    debug('Reached end of key cursor.');
    if (!gotKeyThroughCursor) {
      fail('Did not get key through cursor.');
      return;
    }

    var result = index.openCursor(IDBKeyRange.only(55));
    result.onsuccess = onCursor;
    result.onerror = unexpectedErrorCallback;
    gotObjectThroughCursor = false;
    return;
  }

  debug('Got key through cursor.');
  shouldBe('event.result.key', '55');
  shouldBe('event.result.value', '1');
  gotKeyThroughCursor = true;

  cursor.continue();
}

function getSuccess()
{
  debug('Successfully got object through key in index.');

  shouldBe('event.result.aKey', '55');
  shouldBe('event.result.aValue', '"foo"');

  var result = index.openKeyCursor(IDBKeyRange.only(55));
  result.onsuccess = onKeyCursor;
  result.onerror = unexpectedErrorCallback;
  gotKeyThroughCursor = false;
}

function getKeySuccess()
{
  debug('Successfully got key.');
  shouldBe('event.result', '1');

  var result = index.get(55);
  result.onsuccess = getSuccess;
  result.onerror = unexpectedErrorCallback;
}

function moreDataAdded()
{
  debug('Successfully added more data.');

  var result = index.getKey(55);
  result.onsuccess = getKeySuccess;
  result.onerror = unexpectedErrorCallback;
}

function indexErrorExpected()
{
  debug('Existing index triggered on error as expected.');

  var result = objectStore.put({'aKey': 55, 'aValue': 'foo'}, 1);
  result.onsuccess = moreDataAdded;
  result.onerror = unexpectedErrorCallback;
}

function indexSuccess()
{
  debug('Index created successfully.');

  shouldBe("index.name", "'myIndex'");
  shouldBe("index.storeName", "'test'");
  shouldBe("index.keyPath", "'aKey'");
  shouldBe("index.unique", "true");

  try {
    result = objectStore.createIndex('myIndex', 'aKey', {unique: true});
    fail('Re-creating an index must throw an exception');
  } catch (e) {
    indexErrorExpected();
  }
}

function createIndex(expect_error)
{
  debug('Creating an index.');
  try {
    result = objectStore.createIndex('myIndex', 'aKey', {unique: true});
    window.index = result;
    indexSuccess();
  } catch (e) {
    unexpectedErrorCallback();
  }
}

function dataAddedSuccess()
{
  debug('Data added');
  createIndex(false);
}

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('test');
  var myValue = {'aKey': 21, 'aValue': '!42'};
  var result = objectStore.add(myValue, 0);
  result.onsuccess = dataAddedSuccess;
  result.onerror = unexpectedErrorCallback;
}

function setVersion()
{
  debug('setVersion');
  window.db = event.result;
  var result = db.setVersion('new version');
  result.onsuccess = populateObjectStore;
  result.onerror = unexpectedErrorCallback;
}

function test()
{
  if ('webkitIndexedDB' in window) {
    indexedDB = webkitIndexedDB;
    IDBCursor = webkitIDBCursor;
    IDBKeyRange = webkitIDBKeyRange;
    IDBTransaction = webkitIDBTransaction;
  }

  debug('Connecting to indexedDB');
  var result = indexedDB.open('name');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
