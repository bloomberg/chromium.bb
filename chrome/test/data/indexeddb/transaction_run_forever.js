// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

loopCount = 0;
function endlessLoop()
{
  var request = objectStore.get(0);
  request.onsuccess = endlessLoop;
  request.onerror = unexpectedErrorCallback;

  loopCount += 1;
  if (loopCount == 7) {
    // If we've already looped 7 times, it's pretty safe to assume
    // we'll continue looping for some time...
    debug("Looping infinitely within a transaction.");
    done();
  }
}

function newTransactionComplete()
{
  debug('The transaction completed.');

  var finalTransaction = db.transaction([], IDBTransaction.READ_ONLY);
  finalTransaction.oncomplete = unexpectedCompleteCallback;
  finalTransaction.onabort = unexpectedErrorCallback;

  objectStore = finalTransaction.objectStore('employees');
  endlessLoop();
  endlessLoop(); // Make sure at least one is in flight at any time.
}

function onSetVersionComplete()
{
  debug('Creating new transaction.');
  var newTransaction = db.transaction([], IDBTransaction.READ_WRITE);
  newTransaction.oncomplete = newTransactionComplete;
  newTransaction.onabort = unexpectedAbortCallback;

  var request = newTransaction.objectStore('employees').put(
      {id: 0, name: 'John Doe', desk: 'LON-BEL-123'});
  request.onerror = unexpectedErrorCallback;
}

function onSetVersion()
{
  // We are now in a set version transaction.
  var setVersionTransaction = event.target.result;
  setVersionTransaction.oncomplete = onSetVersionComplete;
  setVersionTransaction.onerror = unexpectedErrorCallback;

  debug('Creating object store.');
  deleteAllObjectStores(db);
  var objectStore = db.createObjectStore('employees', {keyPath: 'id'});
}

function setVersion()
{
  window.db = event.target.result;
  var request = db.setVersion('1.0');
  request.onsuccess = onSetVersion;
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

  debug('Connecting to indexedDB.');
  var request = indexedDB.open('name');
  request.onsuccess = setVersion;
  request.onerror = unexpectedErrorCallback;
}
