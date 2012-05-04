// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  if ('webkitIndexedDB' in window) {
    indexedDB = webkitIndexedDB;
    IDBTransaction = webkitIDBTransaction;
    IDBDatabaseException = webkitIDBDatabaseException;
  }

  debug('Deleting previous database');
  var DBNAME = 'value-size', VERSION = 1;
  var request = indexedDB.deleteDatabase(DBNAME);
  request.onerror = unexpectedErrorCallback;
  request.onsuccess = function () {
    debug('Opening database connection');
    request = indexedDB.open(DBNAME);
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
      db = request.result;
      debug('Setting version');
      request = db.setVersion(VERSION);
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function () {
        debug('Creating object store');
        var store = db.createObjectStore('store');
        store.put("FIRST!!!111!", -Infinity);
        var transaction = request.result;
        transaction.oncomplete = function () {
          setTimeout(testUnderLimit, 0);
        };
      };
    };
  };
}

function repeat(string, len)
{
  return Array(len + 1).join(string);
}

var SEED = "\u0100"; // Encodes to 2 byte in UTF-8 and UTF-16
var BYTES_PER_CHAR = 2;
var CHARS_PER_MB = 1024 * 1024 / BYTES_PER_CHAR;
var SLOP = 1024;
var LIMIT_MB = 64;
var key = 0;
var value;

function testUnderLimit()
{
  debug('Creating test value under limit');
  var underLimit;
  setTimeout(
    function () {
      value = repeat(SEED, LIMIT_MB * CHARS_PER_MB - SLOP); // Just under limit
      underLimit = value;
      setTimeout(doStore, 0);
    }, 0);

  function doStore() {
    var transaction = db.transaction("store", 'readwrite');
    var store = transaction.objectStore("store");
    var request;

    debug("store.add underLimit");
    request = store.add(underLimit, key++);
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
      debug("store.add succeeded");
      debug("store.put underLimit");
      request = store.put(underLimit, key++);
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function () {
        debug("store.put succeeded");
        request = store.openCursor();
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
          var cursor = request.result;
          debug("cursor.update underLimit");
          request = cursor.update(underLimit);
          request.onerror = unexpectedErrorCallback;
          request.onsuccess = function () {
            debug("cursor.update succeeded");
            transaction.abort();
          };
        };
      };
    };

    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = testOverLimit;
  }
}

function testOverLimit()
{
  debug('Creating test value over limit');
  var overLimit;
  setTimeout(
    function () {
      overLimit = value + repeat(SEED, SLOP * 2);
      setTimeout(doStore, 0);
    }, 0);

  function doStore() {
    var transaction = db.transaction("store", 'readwrite');
    var store = transaction.objectStore("store");
    var request;
    try {
      debug("store.add overLimit");
      request = store.add(overLimit, key++);
      request.onsuccess = unexpectedSuccessCallback;
      fail('store.add - Expected exception, but none was raised');
    } catch (e) {
      debug('Exception (expected)');
      code = e.code;
      shouldBe("code", "IDBDatabaseException.DATA_ERR");
    }

    try {
      debug("store.put overLimit");
      request = store.put(overLimit, key++);
      request.onsuccess = unexpectedSuccessCallback;
      fail('store.add - Expected exception, but none was raised');
    } catch (e) {
      debug('Exception (expected)');
      code = e.code;
      shouldBe("code", "IDBDatabaseException.DATA_ERR");
    }

    request = store.openCursor();
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
      var cursor = request.result;
      try {
        debug("cursor.update overLimit");
        request = cursor.update(overLimit);
        request.onerror = unexpectedSuccessCallback;
        fail('cursor.update - Expected exception, but none was raised');
      } catch (e) {
        debug('Exception (expected)');
        code = e.code;
        shouldBe("code", "IDBDatabaseException.DATA_ERR");

        transaction.abort();
      }
    };

    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = function() { done(); };
  }
}
