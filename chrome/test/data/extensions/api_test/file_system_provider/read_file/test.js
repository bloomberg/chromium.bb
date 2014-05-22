// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {number}
 */
var fileSystemId = 0;

/**
 * @type {?DOMFileSystem}
 */
var fileSystem = null;

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object.<number, string>}
 */
var openedFiles = {};

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
 * Testing contents for files.
 * @type {string}
 * @const
 */
var TESTING_TEXT = 'I have a basket full of fruits.';

/**
 * Metadata of a healthy file used to read contents from.
 * @type {Object}
 * @const
 */
var TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: TESTING_TEXT.length,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata of a broken file used to read contents from.
 * @type {Object}
 * @const
 */
var TESTING_BROKEN_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'broken-tiramisu.txt',
  size: TESTING_TEXT.length,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Returns metadata for the requested entry.
 *
 * To successfully acquire a DirectoryEntry, or even a DOMFileSystem, this event
 * must be implemented and return correct values.
 *
 * @param {number} inFileSystemId ID of the file system.
 * @param {string} entryPath Path of the requested entry.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(
    inFileSystemId, entryPath, onSuccess, onError) {
  if (inFileSystemId != fileSystemId) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (entryPath == '/' + TESTING_TIRAMISU_FILE.name) {
    onSuccess(TESTING_TIRAMISU_FILE);
    return;
  }

  if (entryPath == '/' + TESTING_BROKEN_TIRAMISU_FILE.name) {
    onSuccess(TESTING_BROKEN_TIRAMISU_FILE);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
}

/**
 * Requests opening a file at <code>filePath</code>. Further file operations
 * will be associated with the <code>requestId</code>
 *
 * @param {number} inFileSystemId ID of the file system.
 * @param {number} requestId ID of the opening request. Used later for reading.
 * @param {string} filePath Path of the file to be opened.
 * @param {string} mode Mode, either reading or writing.
 * @param {boolean} create True to create if doesn't exist.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onOpenFileRequested(
    inFileSystemId, requestId, filePath, mode, create, onSuccess, onError) {
  if (inFileSystemId != fileSystemId || mode != 'READ' || create) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (filePath == '/' + TESTING_TIRAMISU_FILE.name ||
      filePath == '/' + TESTING_BROKEN_TIRAMISU_FILE.name) {
    openedFiles[requestId] = filePath;
    onSuccess();
  } else {
    onError('NOT_FOUND');  // enum ProviderError.
  }
}

/**
 * Requests closing a file previously opened with <code>openRequestId</code>.
 *
 * @param {number} inFileSystemId ID of the file system.
 * @param {number} openRequestId ID of the request used to open the file.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onCloseFileRequested(
    inFileSystemId, openRequestId, onSuccess, onError) {
  if (inFileSystemId != fileSystemId || !openedFiles[openRequestId]) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  delete openedFiles[requestId];
  onSuccess();
}

/**
 * Requests reading contents of a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {number} inFileSystemId ID of the file system.
 * @param {number} openRequestId ID of the request used to open the file.
 * @param {number} offset Offset of the file.
 * @param {number} length Number of bytes to read.
 * @param {function(ArrayBuffer, boolean)} onSuccess Success callback with a
 *     chunk of data, and information if more data will be provided later.
 * @param {function(string)} onError Error callback.
 */
function onReadFileRequested(
    inFileSystemId, openRequestId, offset, length, onSuccess, onError) {
  var filePath = openedFiles[openRequestId];
  if (inFileSystemId != fileSystemId || !filePath) {
    onError('SECURITY_ERROR');  // enum ProviderError.
    return;
  }

  if (filePath == '/' + TESTING_TIRAMISU_FILE.name) {
    var textToSend = TESTING_TEXT.substr(offset, length);
    var textToSendInChunks = textToSend.split(/(?= )/);

    textToSendInChunks.forEach(function(item, index) {
      // Convert item (string) to an ArrayBuffer.
      var reader = new FileReader();

      reader.onload = function(e) {
        onSuccess(
            e.target.result,
            index < textToSendInChunks.length - 1 /* has_next */);
      };

      reader.readAsArrayBuffer(new Blob([item]));
    });
    return;
  }

  if (filePath == '/' + TESTING_BROKEN_TIRAMISU_FILE.name) {
    onError('ACCESS_DENIED');  // enum ProviderError.
    return;
  }

  onError('INVALID_OPERATION');  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.mount('chocolate.zip', function(id) {
    fileSystemId = id;
    chrome.fileSystemProvider.onGetMetadataRequested.addListener(
        onGetMetadataRequested);
    chrome.fileSystemProvider.onOpenFileRequested.addListener(
        onOpenFileRequested);
    chrome.fileSystemProvider.onReadFileRequested.addListener(
        onReadFileRequested);
    var volumeId =
        'provided:' + chrome.runtime.id + '-' + fileSystemId + '-user';

    chrome.fileBrowserPrivate.requestFileSystem(
        volumeId,
        function(inFileSystem) {
          chrome.test.assertTrue(!!inFileSystem);

          fileSystem = inFileSystem;
          callback();
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
    // Read contents of the /tiramisu.txt file. This file exists, so it should
    // succeed.
    function readFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                var text = fileReader.result;
                chrome.test.assertEq(TESTING_TEXT, text);
                onTestSuccess();
              };
              fileReader.onerror = function(e) {
                chrome.test.fail(fileReader.error.name);
              };
              fileReader.readAsText(file);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },
    // Read contents of a file file, but with an error on the way. This should
    // result in an error.
    function readEntriesError() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_BROKEN_TIRAMISU_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                chrome.test.fail();
              };
              fileReader.onerror = function(e) {
                chrome.test.assertEq('NotReadableError', fileReader.error.name);
                onTestSuccess();
              };
              fileReader.readAsText(file);
            },
            function(error) {
              chrome.test.fail();
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
