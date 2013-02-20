// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  indexedDBTest(prepareDatabase, testUnderLimit);
}

function prepareDatabase()
{
  db = event.target.result;
  debug('Creating object store');
  var store = db.createObjectStore('store');
  store.put("FIRST!!!111!", -Infinity);
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
  debug("");
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
      storeAddSucceeded = true;

      debug("store.put underLimit");
      request = store.put(underLimit, key++);
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function () {
        debug("store.put succeeded");
        storePutSucceeded = true;

        debug("cursor.update underLimit");
        request = store.openCursor();
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
          var cursor = request.result;
          request = cursor.update(underLimit);
          request.onerror = unexpectedErrorCallback;
          request.onsuccess = function () {
            debug("cursor.update succeeded");
            cursorUpdateSucceeded = true;

            transaction.abort();
          };
        };
      };
    };

    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = function() {
      shouldBeTrue("storeAddSucceeded");
      shouldBeTrue("storePutSucceeded");
      shouldBeTrue("cursorUpdateSucceeded");
      testOverLimit();
    };
  }
}

function testOverLimit()
{
  debug("");
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

    debug("store.add overLimit");
    request = store.add(overLimit, key++);
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = function (e) {
      debug("store.add failed: " + e.target.webkitErrorMessage);
      storeAddFailed = true;
      e.preventDefault();

      debug("store.put overLimit");
      request = store.put(overLimit, key++);
      request.onsuccess = unexpectedSuccessCallback;
      request.onerror = function (e) {
        debug("store.put failed: " + e.target.webkitErrorMessage);
        storePutFailed = true;
        e.preventDefault();

        debug("cursor.update overLimit");
        request = store.openCursor();
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
          var cursor = request.result;
          request = cursor.update(overLimit);
          request.onsuccess = unexpectedSuccessCallback;
          request.onerror = function (e) {
            debug("cursor.update failed: " + e.target.webkitErrorMessage);
            cursorUpdateFailed = true;
          };
        };
      };
    };

    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = function() {
      shouldBeTrue("storeAddFailed");
      shouldBeTrue("storePutFailed");
      shouldBeTrue("cursorUpdateFailed");
      done();
    };
  }
}
