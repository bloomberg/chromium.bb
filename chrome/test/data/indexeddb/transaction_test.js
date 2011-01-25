// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function finalTransactionCompleted()
{
  debug('The final transaction completed.');
  done();
}

function finalTransactionAborted()
{
  fail('The final transaction should not abort.');
}

function employeeNotFound()
{
  debug('Employee not found.');
  shouldBe("event.result", "undefined");
}

function newTransactionAborted()
{
  debug('The transaction was aborted.');

  var finalTransaction = db.transaction([], IDBTransaction.READ_ONLY);
  finalTransaction.oncomplete = finalTransactionCompleted;
  finalTransaction.onabort = finalTransactionAborted;

  var request = finalTransaction.objectStore('employees').get(0);
  request.onsuccess = employeeNotFound;
  request.onerror = unexpectedErrorCallback;
}

function newTransactionCompleted()
{
  fail('The new transaction should not complete.');
}

function employeeAdded()
{
  debug('Added an employee inside the transaction.');
}

function onSetVersionComplete()
{
  debug('Creating new transaction.');
  var newTransaction = db.transaction([], IDBTransaction.READ_WRITE);
  newTransaction.oncomplete = newTransactionCompleted;
  newTransaction.onabort = newTransactionAborted;

  var request = newTransaction.objectStore('employees').put(
      {id: 0, name: 'John Doe', desk: 'LON-BEL-123'});
  request.onsuccess = employeeAdded;
  request.onerror = unexpectedErrorCallback;

  newTransaction.abort();
}

function onSetVersion()
{
  // We are now in a set version transaction.
  var setVersionTransaction = event.result;
  setVersionTransaction.oncomplete = onSetVersionComplete;
  setVersionTransaction.onerror = unexpectedErrorCallback;

  debug('Creating object store.');
  deleteAllObjectStores(db);
  var objectStore = db.createObjectStore('employees', {keyPath: 'id'});
}

function setVersion()
{
  window.db = event.result;
  var result = db.setVersion('1.0');
  result.onsuccess = onSetVersion;
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

  debug('Connecting to indexedDB.');
  var result = indexedDB.open('name');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
