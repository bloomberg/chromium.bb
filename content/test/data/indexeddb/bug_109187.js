// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB;

function test() {

  var DBNAME = 'multiEntry-crash-test';
  var request = indexedDB.deleteDatabase(DBNAME);
  request.onsuccess = function (e) {
    request = indexedDB.open(DBNAME);
    request.onsuccess = function (e) {
      var db = request.result;
      request = db.setVersion('1');
      request.onsuccess = function (e) {
        var store = db.createObjectStore('storeName');
        window.index1 = store.createIndex('index1Name', 'prop1');
        window.index2 = store.createIndex(
          'index2Name', 'prop2', {multiEntry: true});
        shouldBeFalse("window.index1.multiEntry");
        shouldBeTrue("window.index2.multiEntry");
        done();
      };
    };
  };
}
