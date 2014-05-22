// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {DOMFileSystem}
 */
var fileSystem = null;

/**
 * @type {string}
 * @const
 */
var FILE_SYSTEM_ID = 'vanilla';

/**
 * @type {Object}
 * @const
 */
var TESTING_ROOT = Object.freeze({
  isDirectory: true,
  name: '',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_HELLO_DIR = Object.freeze({
  isDirectory: true,
  name: 'hello',
  size: 0,
  modificationTime: new Date(2014, 3, 27, 9, 38, 14)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_CANDIES_DIR = Object.freeze({
  isDirectory: true,
  name: 'candies',
  size: 0,
  modificationTime: new Date(2014, 2, 26, 8, 37, 13)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: 1986,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @param {function(Object)} callback Callback to be called on result, with the
 *     volume information object in case of success, or null if not found.
 */
function getVolumeInfo(fileSystemId, callback) {
  chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeList) {
    for (var i = 0; i < volumeList.length; i++) {
      if (volumeList[i].extensionId == chrome.runtime.id &&
          volumeList[i].fileSystemId == fileSystemId) {
        callback(volumeList[i]);
        return;
      }
    }
    callback(null);
  });
}

/**
 * Returns entries in the requested directory.
 *
 * @param {string} inFileSystemId ID of the file system.
 * @param {string} directoryPath Path of the directory.
 * @param {function(Array.<Object>, boolean)} onSuccess Success callback with
 *     a list of entries. May be called multiple times.
 * @param {function(string)} onError Error callback with an error code.
 */
function onReadDirectoryRequested(
    inFileSystemId, directoryPath, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (directoryPath != '/' + TESTING_HELLO_DIR.name) {
    onError('NOT_FOUND');  // enum ProviderError.
    return;
  }

  onSuccess([TESTING_TIRAMISU_FILE], true /* has_next */);
  onSuccess([TESTING_CANDIES_DIR], false /* has_next */);
}

/**
 * Returns metadata for the requested entry.
 *
 * To successfully acquire a DirectoryEntry, or even a DOMFileSystem, this event
 * must be implemented and return correct values.
 *
 * @param {string} inFileSystemId ID of the file system.
 * @param {string} entryPath Path of the requested entry.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(
    inFileSystemId, entryPath, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (entryPath == '/' + TESTING_HELLO_DIR.name) {
    onSuccess(TESTING_HELLO_DIR);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.mount(FILE_SYSTEM_ID, 'chocolate.zip', function() {
    chrome.fileSystemProvider.onReadDirectoryRequested.addListener(
        onReadDirectoryRequested);
    chrome.fileSystemProvider.onGetMetadataRequested.addListener(
        onGetMetadataRequested);

    getVolumeInfo(FILE_SYSTEM_ID, function(volumeInfo) {
      chrome.test.assertTrue(!!volumeInfo);
      chrome.fileBrowserPrivate.requestFileSystem(
          volumeInfo.volumeId,
          function(inFileSystem) {
            chrome.test.assertTrue(!!inFileSystem);

            fileSystem = inFileSystem;
            callback();
          });
    });
  }, function() {
    chrome.test.fail();
  });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Read contents of the /hello directory. This directory exists, so it
    // should succeed.
    function readEntriesSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getDirectory(
          'hello',
          {create: false},
          function(dirEntry) {
            var dirReader = dirEntry.createReader();
            var entries = [];
            var readEntriesNext = function() {
              dirReader.readEntries(function(inEntries) {
                Array.prototype.push.apply(entries, inEntries);
                if (!inEntries.length) {
                  // No more entries, so verify.
                  chrome.test.assertEq(2, entries.length);
                  chrome.test.assertTrue(entries[0].isFile);
                  chrome.test.assertEq('tiramisu.txt', entries[0].name);
                  chrome.test.assertEq(
                      '/hello/tiramisu.txt', entries[0].fullPath);
                  chrome.test.assertTrue(entries[1].isDirectory);
                  chrome.test.assertEq('candies', entries[1].name);
                  chrome.test.assertEq('/hello/candies', entries[1].fullPath);
                  onTestSuccess();
                } else {
                  readEntriesNext();
                }
              }, function(error) {
                chrome.test.fail();
              });
            };
            readEntriesNext();
          },
          function(error) {
            chrome.test.fail();
          });
    },
    // Read contents of a directory which does not exist, what should return an
    // error.
    function readEntriesError() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getDirectory(
          'cranberries',
          {create: false},
          function(dirEntry) {
            chrome.test.fail();
          },
          function(error) {
            chrome.test.assertEq('NotFoundError', error.name);
            onTestSuccess();
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
