// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB;
window.IDBTransaction = window.IDBTransaction || window.webkitIDBTransaction;

window.testResult = '';

function test()
{
  var req = window.indexedDB.open('bug90635');
  req.onerror = function(e) {
    window.testResult = 'fail';
  };
  req.onsuccess = function(e) {
    var db = e.target.result;
    var VERSION = '1';
    if (db.version !== VERSION) {
      var ver = db.setVersion(VERSION);
      ver.onerror = function(e) {
        window.testResult = 'fail';
      };
      ver.onsuccess = function(e) {
        db.createObjectStore('store1');
        db.createObjectStore('store2', {keyPath: ''});
        db.createObjectStore('store3', {keyPath: 'some_path'});
        test_store(db, 'first run');
      };
    } else {
      test_store(db, 'second run');
    }
  };

  function test_store(db, msg) {
    var transaction = db.transaction(['store1', 'store2', 'store3'],
                                     IDBTransaction.READ_ONLY);
    var store1 = transaction.objectStore('store1');
    var store2 = transaction.objectStore('store2');
    var store3 = transaction.objectStore('store3');

    if (store1.keyPath !== null ||
        store2.keyPath !== '' ||
        store3.keyPath !== 'some_path') {
      window.testResult = 'fail - ' + msg;
    } else {
      window.testResult = 'pass - ' + msg;
    }
  }
}
