// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('employees', {keyPath: 'id'});
  shouldBe("objectStore.name", "'employees'");
  shouldBe("objectStore.keyPath", "'id'");

  shouldBe('db.name', '"name"');
  shouldBe('db.version', '"1.0"');
  shouldBe('db.objectStoreNames.length', '1');
  shouldBe('db.objectStoreNames[0]', '"employees"');

  debug('Deleting an object store.');
  db.deleteObjectStore('employees');
  shouldBe('db.objectStoreNames.length', '0');

  done();
}

function setVersion()
{
  debug('setVersion');
  window.db = event.target.result;
  var request = db.setVersion('1.0');
  request.onsuccess = populateObjectStore;
  request.onerror = unexpectedErrorCallback;
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
  var request = indexedDB.open('name');
  request.onsuccess = setVersion;
  request.onerror = unexpectedErrorCallback;
}
