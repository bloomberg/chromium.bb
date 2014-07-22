// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This method initializes the two types of DOM storage.
function initDomStorage(value) {
  window.localStorage.setItem('foo', 'local-' + value);
  window.sessionStorage.setItem('bar', 'session-' + value);
}

// The code below is used for testing IndexedDB isolation.
// The test uses three basic operations -- open, read, write -- to verify proper
// isolation across webview tags with different storage partitions.
// Each of the basic functions below sets document.title to a specific text,
// which the main browser test is waiting for. This is needed because all
// the functions get their results through callbacks and cannot return the
// values directly.
var isolation = {};

isolation.db = null;
isolation.onerror = function(e) {
  document.title = "error";
};

// This method opens the database and creates the objectStore if it doesn't
// exist. It sets the document.title to a string referring to which
// operation has been performed - open vs create.
function initIDB() {
  var v = 3;
  var ranVersionChangeTransaction = false;
  var request = indexedDB.open("isolation", v);
  request.onupgradeneeded = function(e) {
    isolation.db = e.target.result;
    var store = isolation.db.createObjectStore(
        "partitions", {keyPath: "id"});
    e.target.transaction.oncomplete = function() {
      ranVersionChangeTransaction = true;
    }
  }
  request.onsuccess = function(e) {
    isolation.db = e.target.result;
    if (ranVersionChangeTransaction) {
      document.title = "idb created";
    } else {
      document.title = "idb open";
    }
  };
  request.onerror = isolation.onerror;
  request.onblocked = isolation.onerror;
}

// This method adds a |value| to the database identified by |id|.
function addItemIDB(id, value) {
  var trans = isolation.db.transaction(["partitions"], "readwrite");
  var store = trans.objectStore("partitions");
  var data = { "partition": value, "id": id };

  var request = store.put(data);
  request.onsuccess = function(e) {
    document.title = "addItemIDB complete";
  };
  request.onerror = isolation.onerror;
};

var storedValue = null;

// This method reads an item from the database, identified by |id|. Since
// the value cannot be returned directly, it is saved into the global
// "storedValue" variable, which is then read through getValueIDB().
function readItemIDB(id) {
  storedValue = null;
  var trans = isolation.db.transaction(["partitions"], "readwrite");
  var store = trans.objectStore("partitions");

  var request = store.get(id);
  request.onsuccess = function(e) {
    if (!!e.target.result != false) {
      storedValue = request.result.partition;
    }
    document.title = "readItemIDB complete";
  };
  request.onerror = isolation.onerror;
}

function getValueIDB() {
  return storedValue;
}
