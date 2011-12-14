// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB;
window.IDBTransaction = window.IDBTransaction || window.webkitIDBTransaction;

window.testResult = '';

function unexpectedErrorCallback()
{
  document.title = 'fail - unexpected error callback';
}
function unexpectedAbortCallback()
{
  document.title = 'fail - unexpected abort callback';
}
function unexpectedCompleteCallback()
{
  document.title = 'fail - unexpected complete callback';
}

function test() {
  if (document.location.hash === '#part1') {
    testPart1();
  } else if (document.location.hash === '#part2') {
    testPart2();
  } else if (document.location.hash === '#part3') {
    testPart3();
  } else {
    document.title = 'fail';
  }
}

function testPart1() {
  // Prepare the database, then exit normally

  // Set version 1, create store1
  var delreq = window.indexedDB.deleteDatabase('version-change-crash');
  delreq.onerror = unexpectedErrorCallback;
  delreq.onsuccess = function() {
    var openreq = window.indexedDB.open('version-change-crash');
    openreq.onerror = unexpectedErrorCallback;
    openreq.onsuccess = function(e) {
      var db = openreq.result;
      var setverreq = db.setVersion('1');
      setverreq.onerror = unexpectedErrorCallback;
      setverreq.onsuccess = function(e) {
        var transaction = setverreq.result;
        db.createObjectStore('store1');
        transaction.onabort = unexpectedAbortCallback;
        transaction.oncomplete = function (e) {
          document.title = 'pass - part1 - complete';
        };
      };
    };
  };
}

function testPart2() {
  // Start a VERSION_CHANGE then crash

  // Set version 2, twiddle stores and crash
  var openreq = window.indexedDB.open('version-change-crash');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onsuccess = function(e) {
    var db = openreq.result;
    var setverreq = db.setVersion('2');
    setverreq.onerror = unexpectedErrorCallback;
    setverreq.onsuccess = function(e) {
      var transaction = setverreq.result;
      transaction.onabort = unexpectedAbortCallback;
      transaction.oncomplete = unexpectedCompleteCallback;

      var store = db.createObjectStore('store2');
      document.title = 'pass - part2 - crash me';

      // Keep adding to the transaction so it can't commit
      (function loop() { store.put(0, 0).onsuccess = loop; }());
    };
  };
}

function testPart3() {
  // Validate that Part 2 never committed

  // Check version
  var openreq = window.indexedDB.open('version-change-crash');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onsuccess = function(e) {
    var db = openreq.result;
    if (db.version !== '1') {
      document.title = 'fail - version incorrect';
      return;
    }

    if (!db.objectStoreNames.contains('store1')) {
      document.title = 'fail - store1 does not exist';
      return;
    }

    if (db.objectStoreNames.contains('store2')) {
      document.title = 'fail - store2 exists';
      return;
    }

    document.title = 'pass - part3 - rolled back';
  };
}
