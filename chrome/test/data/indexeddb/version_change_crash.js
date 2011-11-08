// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB;
window.IDBTransaction = window.IDBTransaction || window.webkitIDBTransaction;

window.testResult = '';

var INDUCE_BROWSER_CRASH_URL = 'about:inducebrowsercrashforrealz';
function crashBrowser() {
  chrome.tabs.create({url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
}


function test() {

  var openreq = window.indexedDB.open('test-db');
  openreq.onsuccess = function(e) {
    var db = openreq.result;

    if (document.location.hash === '#part1') {
      testPart1(db);
    } else if (document.location.hash === '#part2') {
      testPart2(db);
    } else if (document.location.hash === '#part3') {
      testPart3(db);
    } else {
      window.testResult = 'fail';
    }
  };
}

function testPart1(db) {
  // Prepare the database, then exit normally

  // Set version 1, create store1
  var setverreq = db.setVersion('1.0');
  setverreq.onsuccess = function(e) {
    db.createObjectStore('store1');
    setverreq.result.oncomplete = function (e) {
      window.testResult = 'part1 - complete';
    };
  };
}

function testPart2(db) {
  // Start a VERSION_CHANGE then crash

  // Set version 2, twiddle stores and crash
  var setverreq = db.setVersion('2.0');
  setverreq.onsuccess = function(e) {
    var store = db.createObjectStore('store2');
    window.testResult = 'part2 - crash me';

    // Keep adding to the transaction so it can't commit
    (function loop() { store.put(0, 0).onsuccess = loop; }());
  };
}

function testPart3(db) {
  // Validate that Part 2 never committed

  // Check version
  if (db.version !== '1.0') {
    window.testResult = 'fail, version incorrect';
    return;
  }

  if (!db.objectStoreNames.contains('store1')) {
    window.testResult = 'fail, store1 does not exist';
    return;
  }

  if (db.objectStoreNames.contains('store2')) {
    window.testResult = 'fail, store2 exists';
    return;
  }

  window.testResult = 'part3 - pass';
}
