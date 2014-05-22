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
  modificationTime: new Date(2013, 3, 27, 9, 38, 14)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_WRONG_TIME_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-time.txt',
  size: 4096,
  modificationTime: new Date('Invalid date.')
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
 * Returns metadata for a requested entry.
 *
 * @param {string} inFileSystemId ID of the file system.
 * @param {string} entryPath Path of the requested entry.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(inFileSystemId, entryPath, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (entryPath == '/' + TESTING_FILE.name) {
    onSuccess(TESTING_FILE);
    return;
  }

  if (entryPath == '/' + TESTING_WRONG_TIME_FILE.name) {
    onSuccess(TESTING_WRONG_TIME_FILE);
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
    // Read metadata of the root.
    function getFileMetadataSuccess() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getMetadata(
        function(metadata) {
          chrome.test.assertEq(TESTING_ROOT.size, metadata.size);
          chrome.test.assertEq(
              TESTING_ROOT.modificationTime.toString(),
              metadata.modificationTime.toString());
          onSuccess();
        }, function(error) {
          chrome.test.fail(error.name);
        });
    },
    // Read metadata of an existing testing file.
    function getFileMetadataSuccess() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.test.assertEq(
                TESTING_FILE.isDirectory, fileEntry.isDirectory);
            fileEntry.getMetadata(function(metadata) {
              chrome.test.assertEq(TESTING_FILE.size, metadata.size);
              chrome.test.assertEq(
                  TESTING_FILE.modificationTime.toString(),
                  metadata.modificationTime.toString());
              onSuccess();
            }, function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },
    // Read metadata of an existing testing file, which however has an invalid
    // modification time. It should not cause an error, but an invalid date
    // should be passed to fileapi instead. The reason is, that there is no
    // easy way to verify an incorrect modification time at early stage.
    function getFileMetadataWrongTimeSuccess() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_WRONG_TIME_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.assertEq(TESTING_WRONG_TIME_FILE.name, fileEntry.name);
            fileEntry.getMetadata(function(metadata) {
              chrome.test.assertTrue(
                  Number.isNaN(metadata.modificationTime.getTime()));
              onSuccess();
            }, function(error) {
              chrome.test.fail(error.name);
            });
          }, function(error) {
            chrome.test.fail(error.name);
          });
    },
    // Read metadata of a directory which does not exist, what should return an
    // error. DirectoryEntry.getDirectory() causes fetching metadata.
    function getFileMetadataNotFound() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getDirectory(
          'cranberries',
          {create: false},
          function(dirEntry) {
            chrome.test.fail();
          },
          function(error) {
            chrome.test.assertEq('NotFoundError', error.name);
            onSuccess();
          });
    },
    // Read metadata of a file using getDirectory(). An error should be returned
    // because of type mismatching. DirectoryEntry.getDirectory() causes
    // fetching metadata.
    function getFileMetadataWrongType() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getDirectory(
          TESTING_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.fail();
          },
          function(error) {
            chrome.test.assertEq('TypeMismatchError', error.name);
            onSuccess();
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
