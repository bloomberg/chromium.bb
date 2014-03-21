// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

var kSavedKey = 'ephemeral';
var kSavedValue = 'app';
var kTestFileName = 'ephemeral.txt';
var kTestDBName = 'ephemeral_db';

function FileSystemWriteError() {
  chrome.test.fail('Filesystem write error');
}

function FileSystemReadError() {
  chrome.test.fail('Filesystem read error');
}

function IndexDBError() {
  chrome.test.fail('IndexDB error');
}

function WriteLocalStorage() {
  var data = {};
  data[kSavedKey] = kSavedValue;
  chrome.storage.local.set(data, callbackPass(function() {}));
}

function WriteFileSystem() {
  // Testing the existence of a file is sufficient.
  window.webkitRequestFileSystem(
      PERSISTENT,
      512,
      callbackPass(function(fs) {
        fs.root.getFile(
            kTestFileName,
            {create: true, exclusive: true},
            callbackPass(function(fileEntry) {
              // Succeeded
            }),
            FileSystemWriteError);
      }), FileSystemWriteError);
}

function WriteIndexedDB() {
  var openDB = indexedDB.open(kTestDBName, 1);
  openDB.onerror = IndexDBError;

  openDB.onsuccess = callbackPass(function(e) {
    var db = e.target.result;
    var transaction = db.transaction([kTestDBName], 'readwrite');
    var store = transaction.objectStore(kTestDBName);

    var request = store.add(kSavedValue, kSavedKey);
    request.onerror = IndexDBError;

    request.onsuccess = callbackPass(function(e) {
      // Succeeded
    });
  });

  openDB.onupgradeneeded = function(e) {
    var db = e.target.result;
    db.createObjectStore(kTestDBName);
  };
}

function ReadLocalStorage() {
  chrome.storage.local.get(kSavedKey, callbackPass(function(items) {
    chrome.test.assertTrue(typeof(items[kSavedKey]) !== 'undefined');
    chrome.test.assertEq(kSavedValue, items[kSavedKey]);
  }));
}

function ReadFileSystem() {
  window.webkitRequestFileSystem(
      PERSISTENT,
      512,
      callbackPass(function(fs) {
        fs.root.getFile(
            kTestFileName,
            {},
            callbackPass(function(fileEntry) {
              // Succeeded
            }),
            FileSystemReadError);
      }),
      FileSystemReadError);
}

function ReadIndexedDB() {
  var openDB = indexedDB.open(kTestDBName, 1);
  openDB.onerror = IndexDBError;

  openDB.onsuccess = callbackPass(function(e) {
    var db = e.target.result;
    var transaction = db.transaction([kTestDBName], 'readonly');
    var store = transaction.objectStore(kTestDBName);

    var request = store.get(kSavedKey);
    request.onerror = IndexDBError;

    request.onsuccess = callbackPass(function(e) {
      chrome.test.assertEq(kSavedValue, e.target.result);
    });
  });

  openDB.onupgradeneeded = function(e) {
    chrome.test.fail('Indexed DB not initialized');
  };
}

function CheckLocalStorageReset() {
  chrome.storage.local.get(kSavedKey, callbackPass(function(items) {
    chrome.test.assertEq('undefined', typeof(items[kSavedKey]));
  }));
}

function CheckFileSystemReset() {
  window.webkitRequestFileSystem(
      PERSISTENT,
      512,
      callbackPass(function(fs) {
        fs.root.getFile(
            kTestFileName,
            {},
            function(fileEntry) {
              chrome.test.fail('File ' + kTestFileName + ' should not exist');
            },
            callbackPass(function(e) {
              // Expected failure
            }));
      }),
      FileSystemReadError);
}

function CheckIndexedDBReset() {
  var openDB = indexedDB.open(kTestDBName, 1);
  openDB.onerror = IndexDBError;

  openDB.onsuccess = callbackPass(function(e) {
    var db = e.target.result;
    chrome.test.assertFalse(db.objectStoreNames.contains(kTestDBName));
  });
}

// Phase 1 - Write data to various storage types.
function WriteData() {
  chrome.test.runTests([
    WriteLocalStorage,
    WriteFileSystem,
    WriteIndexedDB
  ]);
}

// Phase 2 - Read data back from the various storage types.
function ReadData() {
  chrome.test.runTests([
    ReadLocalStorage,
    ReadFileSystem,
    ReadIndexedDB
  ]);
}

// Verify that all data has been reset.
function DataReset() {
  chrome.test.runTests([
    CheckLocalStorageReset,
    CheckFileSystemReset,
    CheckIndexedDBReset
  ]);
}

onload = function() {
  chrome.test.sendMessage('launched', function(reply) {
    window[reply]();
  });
};
