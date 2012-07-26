// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB ||
    window.mozIndexedDB || window.msIndexedDB;

var automation = {
  results: {}
};

automation.setDone = function() {
  this.setStatus("Test complete.");
  document.cookie = '__done=1; path=/';
};

automation.addResult = function(name, result) {
  result = "" + result;
  this.results[name] = result;
  var elt = document.getElementById('results');
  var div = document.createElement('div');
  div.innerText = name + ": " + result;
  elt.appendChild(div);
};

automation.getResults = function() {
  return this.results;
};

automation.setStatus = function(s) {
  document.getElementById('status').innerText = s;
};

function assert(t) {
  if (!t) {
    var e = new Error("Assertion failed!");
    console.log(e.stack);
    throw e;
  }
}

function onError(e) {
  var s = "Caught error.";
  if (e.target && e.target.webkitErrorMessage)
    s += "\n" + e.target.webkitErrorMessage;
  console.log(s);
  automation.setStatus(s);
  e.stopPropagation();
  throw new Error(e);
}

var version = 2;  // The version with our object stores.

// Valid options fields:
//  indexName: the name of an index to create on each object store
//  indexKeyPath: likewise
//  indexIsUnique: the "unique" option for IDBIndexParameters
//  indexIsMultiEntry: the "multiEntry" option for IDBIndexParameters
//
function createDatabase(
    name, objectStoreNames, handler, errorHandler, options) {
  var openRequest = indexedDB.open(name, version);
  openRequest.onblocked = errorHandler;
  function createObjectStores(db) {
    for (var store in objectStoreNames) {
      var name = objectStoreNames[store];
      assert(!db.objectStoreNames.contains(name));
      var os = db.createObjectStore(name);
      if (options && options.indexName) {
        assert('indexKeyPath' in options);
        os.createIndex(options.indexName, options.indexKeyPath,
            { unique: options.indexIsUnique,
              multiEntry: options.indexIsMultiEntry });
      }
    }
  }
  openRequest.onupgradeneeded = function(ev) {
    // TODO: This is the spec-compliant path, which doesn't yet work in Chrome,
    // and isn't yet tested, as this function won't currently be called.
    assert(openRequest == ev.target);
    createObjectStores(openRequest.result);
    // onsuccess will get called after this exits.
  };
  openRequest.onsuccess = function(ev) {
    assert(openRequest == ev.target);
    var db = openRequest.result;
    db.onerror = function(ev) {
      console.log("db error", arguments, openRequest.webkitErrorMessage);
      errorHandler();
    };
    if (db.version != version) {
      // This is the current Chrome path.
      var setVersionRequest = db.setVersion(version);
      setVersionRequest.onfailure = errorHandler;
      setVersionRequest.onsuccess = function(e) {
        assert(setVersionRequest == e.target);
        createObjectStores(db);
        var versionTransaction = setVersionRequest.result;
        versionTransaction.oncomplete = function() {handler(db); };
        versionTransaction.onerror = onError;
      };
    } else {
      handler(db);
    }
  }
}

function getTransaction(db, objectStoreNames, mode, opt_handler) {
  var transaction = db.transaction(objectStoreNames, mode);
  transaction.onerror = onError;
  transaction.onabort = onError;
  if (opt_handler) {
    transaction.oncomplete = opt_handler;
  }
  return transaction;
}

function deleteDatabase(name, opt_handler) {
  var deleteRequest = indexedDB.deleteDatabase(name);
  deleteRequest.onerror = onError;
  if (opt_handler) {
    deleteRequest.onsuccess = opt_handler;
  }
}

function getCompletionFunc(testName, startTime, onTestComplete) {
  function onDeleted() {
    automation.setStatus("Deleted database.");
    onTestComplete();
  }
  return function() {
    var duration = Date.now() - startTime;
    // Ignore the cleanup time for this test.
    automation.addResult(testName, duration);
    automation.setStatus("Deleting database.");
    // TODO: Turn on actual deletion; for now it's way too slow.
    // deleteDatabase(testName, onDeleted);
    onTestComplete();
  }
}

function stringOfLength(n) {
  assert(n > 0);
  assert(n == Math.floor(n));
  return new Array(n + 1).join('0');
}
