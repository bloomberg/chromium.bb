// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object.<number, string>}
 */
var openedFiles = {};

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
 * Requests reading contents of a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {ReadFileRequestedOptions} options Options.
 * @param {function(ArrayBuffer, boolean)} onSuccess Success callback with a
 *     chunk of data, and information if more data will be provided later.
 * @param {function(string)} onError Error callback.
 */
function onReadFileRequested(options, onSuccess, onError) {
  var filePath = test_util.openedFiles[options.openRequestId];
  if (options.fileSystemId != test_util.FILE_SYSTEM_ID || !filePath) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (filePath == '/' + TESTING_TIRAMISU_FILE.name) {
    var textToSend = TESTING_TEXT.substr(options.offset, options.length);
    var textToSendInChunks = textToSend.split(/(?= )/);

    textToSendInChunks.forEach(function(item, index) {
      // Convert item (string) to an ArrayBuffer.
      var reader = new FileReader();

      reader.onload = function(e) {
        onSuccess(
            e.target.result,
            index < textToSendInChunks.length - 1 /* hasMore */);
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
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      test_util.onOpenFileRequested);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      test_util.onCloseFileRequested);

  test_util.defaultMetadata['/' + TESTING_TIRAMISU_FILE.name] =
      TESTING_TIRAMISU_FILE;
  test_util.defaultMetadata['/' + TESTING_BROKEN_TIRAMISU_FILE.name] =
      TESTING_BROKEN_TIRAMISU_FILE;

  chrome.fileSystemProvider.onReadFileRequested.addListener(
      onReadFileRequested);

  test_util.mountFileSystem(callback);
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
      test_util.fileSystem.root.getFile(
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
      test_util.fileSystem.root.getFile(
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
