// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  if (document.location.hash === '#part1') {
    testPart1();
  } else if (document.location.hash === '#part2') {
    testPart2();
  } else {
    result('fail - unexpected hash');
  }
}

function testPart1()
{
  var delreq = window.indexedDB.deleteDatabase('bug90635');
  delreq.onerror = unexpectedErrorCallback;
  delreq.onsuccess = function() {
    var openreq = window.indexedDB.open('bug90635');
    openreq.onerror = unexpectedErrorCallback;
    openreq.onsuccess = function(e) {
      var db = openreq.result;
      var setverreq = db.setVersion('1');
      setverreq.onerror = unexpectedErrorCallback;
      setverreq.onsuccess = function(e) {
        var transaction = setverreq.result;

        db.createObjectStore('store1');
        db.createObjectStore('store2', {keyPath: ''});
        db.createObjectStore('store3', {keyPath: 'some_path'});

        transaction.onabort = unexpectedAbortCallback;
        transaction.oncomplete = function() {
          test_store(db, 'first run');
        };
      };
    };
  };
}

function testPart2()
{
  var openreq = window.indexedDB.open('bug90635');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onsuccess = function(e) {
    var db = openreq.result;
    test_store(db, 'second run');
  };
}

function test_store(db, msg) {
  var transaction = db.transaction(['store1', 'store2', 'store3'], 'readonly');
  var store1 = transaction.objectStore('store1');
  var store2 = transaction.objectStore('store2');
  var store3 = transaction.objectStore('store3');

  if (store1.keyPath !== null ||
      store2.keyPath !== '' ||
      store3.keyPath !== 'some_path') {
    result('fail - ' + msg);
  } else {
    result('pass - ' + msg);
  }
}
