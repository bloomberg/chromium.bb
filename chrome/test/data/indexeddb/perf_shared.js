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
  console.log(e);
  e.stopPropagation();
  throw new Error(e);
}

var version = 2;  // The version with our object stores.
var db;

function createDatabase(name, objectStores, handler, errorHandler) {
  var openRequest = indexedDB.open(name, version);
  openRequest.onblocked = errorHandler;
  function createObjectStores(db) {
    for (var store in objectStores) {
      var name = objectStores[store];
      assert(!db.objectStoreNames.contains(name));
      db.createObjectStore(name);
    }
  }
  openRequest.onupgradeneeded = function(ev) {
    // TODO: This is the spec-compliant path, which doesn't yet work in Chrome,
    // and isn't yet tested, as this function won't currently be called.
    assert(openRequest == ev.target);
    db = openRequest.result;
    createObjectStores(db);
    // onsuccess will get called after this exits.
  };
  openRequest.onsuccess = function(ev) {
    assert(openRequest == ev.target);
    db = openRequest.result;
    db.onerror = function(ev) {
      console.log("db error", arguments, openRequest.webkitErrorMessage);
      errorHandler();
    }
    if (db.version != version) {
      // This is the current Chrome path.
      var setVersionRequest = db.setVersion(version);
      setVersionRequest.onfailure = errorHandler;
      setVersionRequest.onsuccess =
          function(e) {
            assert(setVersionRequest == e.target);
            createObjectStores(db);
            var versionTransaction = setVersionRequest.result;
            versionTransaction.oncomplete = function() {handler(db); };
            versionTransaction.onerror = onError;
          }
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

function cleanUp(opt_handler) {
  if (db) {
    deleteDatabase(db, opt_handler);
    db = null;
  }
}

function stringOfLength(n) {
  assert(n > 0);
  assert(n == Math.floor(n));
  return new Array(n + 1).join('0');
}
