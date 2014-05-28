// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {DOMFileSystem}
 */
var fileSystem = null;

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object.<number, string>}
 */
var openedFiles = {};

/**
 * @type {string}
 * @const
 */
var FILE_SYSTEM_ID = 'chocolate-id';

/**
 * @type {string}
 * @const
 */
var TESTING_TEXT = 'We are bytes at the 5th GB of the file!';

/**
 * @type {number}
 * @const
 */
var TESTING_TEXT_OFFSET = 5 * 1000 * 1000 * 1000;

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
 * Metadata for a testing file with 6GB file size.
 * @type {Object}
 * @const
 */
var TESTING_6GB_FILE = Object.freeze({
  isDirectory: false,
  name: '6gb.txt',
  size: 6 * 1024 * 1024 * 1024,
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
function onGetMetadataRequested(inFileSystemId, entryPath, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (entryPath == '/' + TESTING_6GB_FILE.name) {
    onSuccess(TESTING_6GB_FILE);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
}

/**
 * Requests opening a file at <code>filePath</code>. Further file operations
 * will be associated with the <code>requestId</code>
 *
 * @param {string} inFileSystemId ID of the file system.
 * @param {number} requestId ID of the opening request. Used later for reading.
 * @param {string} filePath Path of the file to be opened.
 * @param {string} mode Mode, either reading or writing.
 * @param {boolean} create True to create if doesn't exist.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onOpenFileRequested(
    inFileSystemId, requestId, filePath, mode, create, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (mode != 'READ' || create) {
    onError('ACCESS_DENIED');  // enum ProviderError.
    return;
  }

  if (filePath != '/' + TESTING_6GB_FILE.name) {
    onError('NOT_FOUND');  // enum ProviderError.
    return;
  }

  openedFiles[requestId] = filePath;
  onSuccess();
}

/**
 * Requests closing a file previously opened with <code>openRequestId</code>.
 *
 * @param {string} inFileSystemId ID of the file system.
 * @param {number} openRequestId ID of the request used to open the file.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onCloseFileRequested(
    inFileSystemId, openRequestId, onSuccess, onError) {
  if (inFileSystemId != FILE_SYSTEM_ID || !openedFiles[openRequestId]) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  delete openedFiles[requestId];
  onSuccess();
}

/**
 * Requests reading contents of a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {string} inFileSystemId ID of the file system.
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
  if (inFileSystemId != FILE_SYSTEM_ID || !filePath) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (filePath == '/' + TESTING_6GB_FILE.name) {
    if (offset < TESTING_TEXT_OFFSET ||
        offset + length > TESTING_TEXT_OFFSET + TESTING_TEXT.length) {
      console.error('Reading from a wrong location in the file!');
      onError('INVALID_FAILED');  // enum ProviderError.
      return;
    }

    var buffer = TESTING_TEXT.substr(offset - TESTING_TEXT_OFFSET, length);
    var reader = new FileReader();
    reader.onload = function(e) {
      onSuccess(e.target.result, false /* hasMore */);
    };
    reader.readAsArrayBuffer(new Blob([buffer]));
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
  chrome.fileSystemProvider.mount(FILE_SYSTEM_ID, 'chocolate.zip', function() {
    chrome.fileSystemProvider.onGetMetadataRequested.addListener(
        onGetMetadataRequested);
    chrome.fileSystemProvider.onOpenFileRequested.addListener(
        onOpenFileRequested);
    chrome.fileSystemProvider.onReadFileRequested.addListener(
        onReadFileRequested);
    var volumeId =
        'provided:' + chrome.runtime.id + '-' + FILE_SYSTEM_ID + '-user';

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
    // Read contents of a new file, which is 6GB in size. Such size does not
    // fit in a 32bit int nor in size_t (unsigned). It should be safe to assume,
    // that if 6GB are supported, then there is no 32bit bottle neck, and the
    // next one would be 64bit. File System Provider API should support files
    // with size greater or equal to 2^53.
    function readBigFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_6GB_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              // Read 10 bytes from the 5th GB.
              var fileSlice =
                  file.slice(TESTING_TEXT_OFFSET,
                             TESTING_TEXT_OFFSET + TESTING_TEXT.length);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                var text = fileReader.result;
                chrome.test.assertEq(TESTING_TEXT, text);
                onTestSuccess();
              };
              fileReader.onerror = function(e) {
                chrome.test.fail(fileReader.error.name);
              };
              fileReader.readAsText(fileSlice);
            },
            function(error) {
              chrome.test.fail(error.name);
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
