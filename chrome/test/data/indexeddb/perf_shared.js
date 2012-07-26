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

var baseVersion = 2;  // The version with our object stores.
var curVersion;

// Valid options fields:
//  indexName: the name of an index to create on each object store
//  indexKeyPath: the key path for that index
//  indexIsUnique: the "unique" option for IDBIndexParameters
//  indexIsMultiEntry: the "multiEntry" option for IDBIndexParameters
//
function createDatabase(
    name, objectStoreNames, handler, errorHandler, options) {
  var openRequest = indexedDB.open(name, baseVersion);
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
    }
    if (db.version != baseVersion) {
      // This is the current Chrome path.
      var setVersionRequest = db.setVersion(baseVersion);
      setVersionRequest.onerror = errorHandler;
      setVersionRequest.onsuccess = function(e) {
        curVersion = db.version;
        assert(setVersionRequest == e.target);
        createObjectStores(db);
        var versionTransaction = setVersionRequest.result;
        versionTransaction.oncomplete = function() { handler(db); };
        versionTransaction.onerror = onError;
      }
    } else {
      handler(db);
    }
  }
}

// You must close all database connections before calling this.
function alterObjectStores(
    name, objectStoreNames, func, handler, errorHandler) {
  var version = curVersion + 1;
  var openRequest = indexedDB.open(name, version);
  openRequest.onblocked = errorHandler;
  // TODO: This won't work in Firefox yet; see above in createDatabase.
  openRequest.onsuccess = function(ev) {
    assert(openRequest == ev.target);
    var db = openRequest.result;
    db.onerror = function(ev) {
      console.log("error altering db", arguments,
          openRequest.webkitErrorMessage);
      errorHandler();
    }
    if (db.version != version) {
      var setVersionRequest = db.setVersion(version);
      setVersionRequest.onerror = errorHandler;
      setVersionRequest.onsuccess =
          function(e) {
            curVersion = db.version;
            assert(setVersionRequest == e.target);
            var versionTransaction = setVersionRequest.result;
            versionTransaction.oncomplete = function() { handler(db); };
            versionTransaction.onerror = onError;
            for (var store in objectStoreNames) {
              func(versionTransaction.objectStore(objectStoreNames[store]));
            }
          }
    } else {
      errorHandler();
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

function getDisplayName(args) {
  // The last arg is the completion callback the test runner tacks on.
  return getDisplayName.caller.name + "_" +
      Array.prototype.slice.call(args, 0, args.length - 1).join("_");
}

function getSimpleKey(i) {
  return "key " + i;
}

function getSimpleValue(i) {
  return "value " + i;
}

function putLinearValues(
    transaction, objectStoreNames, numKeys, getKey, getValue) {
  if (!getKey)
    getKey = getSimpleKey;
  if (!getValue)
    getValue = getSimpleValue;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    for (var j = 0; j < numKeys; ++j) {
      var request = os.put(getValue(j), getKey(j));
      request.onerror = onError;
    }
  }
}

function getRandomValues(
    transaction, objectStoreNames, numReads, numKeys, indexName, getKey) {
  if (!getKey)
    getKey = getSimpleKey;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    var source = os;
    if (indexName)
      source = source.index(indexName);
    for (var j = 0; j < numReads; ++j) {
      var rand = Math.floor(Math.random() * numKeys);
      var request = source.get(getKey(rand));
      request.onerror = onError;
    }
  }
}

function putRandomValues(
    transaction, objectStoreNames, numPuts, numKeys, getKey, getValue) {
  if (!getKey)
    getKey = getSimpleKey;
  if (!getValue)
    getValue = getSimpleValue;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    for (var j = 0; j < numPuts; ++j) {
      var rand = Math.floor(Math.random() * numKeys);
      var request = os.put(getValue(rand), getKey(rand));
      request.onerror = onError;
    }
  }
}

function stringOfLength(n) {
  assert(n > 0);
  assert(n == Math.floor(n));
  return new Array(n + 1).join('0');
}
