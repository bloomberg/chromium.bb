// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Map from a file path to contents of the file.
 * @type {Object.<string, string>}
 */
var fileContents = {};

/**
 * Initial contents of testing files.
 * @type {string}
 * @const
 */
var TESTING_INITIAL_TEXT = "Hello world. How are you today?";

/**
 * Initial contents of testing files.
 * @type {string}
 ";
 */
var TESTING_TEXT_TO_WRITE = "Vanilla ice creams are the best.";

/**
 * @type {string}
 * @const
 */
var TESTING_NEW_FILE_NAME = "perfume.txt"

/**
 * @type {string}
 * @const
 */
var TESTING_TIRAMISU_FILE_NAME = 'tiramisu.txt';

/**
 * @type {string}
 * @const
 */
var TESTING_BROKEN_TIRAMISU_FILE_NAME = 'broken-tiramisu.txt';

/**
 * Requests writing contents to a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {ReadFileRequestedOptions} options Options.
 * @param {function(} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onWriteFileRequested(options, onSuccess, onError) {
  var filePath = test_util.openedFiles[options.openRequestId];
  if (options.fileSystemId != test_util.FILE_SYSTEM_ID || !filePath) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (!(filePath in test_util.defaultMetadata)) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  var metadata = test_util.defaultMetadata[filePath];

  if (filePath == '/' + TESTING_BROKEN_TIRAMISU_FILE_NAME) {
    onError('FAILED');
    return;
  }

  // Writing beyond the end of the file.
  if (options.offset > metadata.size) {
    onError('INVALID_OPERATION');
    return;
  }

  // Convert ArrayBuffer to string.
  var reader = new FileReader();
  reader.onloadend = function(e) {
    var oldContents = fileContents[filePath] || '';
    var newContents = oldContents.substr(0, options.offset) + reader.result +
        oldContents.substr(options.offset + reader.result.length);
    metadata.size = newContents.length;
    fileContents[filePath] = newContents;
    onSuccess();
  }

  reader.readAsText(new Blob([options.data]));
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
  chrome.fileSystemProvider.onCreateFileRequested.addListener(
      test_util.onCreateFileRequested);

  test_util.defaultMetadata['/' + TESTING_TIRAMISU_FILE_NAME] = {
    isDirectory: false,
    name: TESTING_TIRAMISU_FILE_NAME,
    size: TESTING_INITIAL_TEXT.length,
    modificationTime: new Date(2014, 1, 24, 6, 35, 11)
  };

  test_util.defaultMetadata['/' + TESTING_BROKEN_TIRAMISU_FILE_NAME] = {
    isDirectory: false,
    name: TESTING_BROKEN_TIRAMISU_FILE_NAME,
    size: TESTING_INITIAL_TEXT.length,
    modificationTime: new Date(2014, 1, 25, 7, 36, 12)
  };

  fileContents['/' + TESTING_TIRAMISU_FILE_NAME] = TESTING_INITIAL_TEXT;
  fileContents['/' + TESTING_BROKEN_TIRAMISU_FILE_NAME] = TESTING_INITIAL_TEXT;

  chrome.fileSystemProvider.onWriteFileRequested.addListener(
      onWriteFileRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Write contents to a non-existing file. It should succeed.
    function writeNewFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_NEW_FILE_NAME,
          {create: true, exclusive: true},
          function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.onwriteend = function(e) {
                // Note that onwriteend() is called even if an error happened.
                if (fileWriter.error)
                  return;
                chrome.test.assertEq(
                    TESTING_TEXT_TO_WRITE,
                    fileContents['/' + TESTING_NEW_FILE_NAME]);
                onTestSuccess();
              };
              fileWriter.onerror = function(e) {
                chrome.test.fail(fileWriter.error.name);
              };
              var blob = new Blob(
                  [TESTING_TEXT_TO_WRITE], {type: 'text/plain'});
              fileWriter.write(blob);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Overwrite contents in an existing file. It should succeed.
    function overwriteFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE_NAME,
          {create: true, exclusive: false},
          function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.onwriteend = function(e) {
                if (fileWriter.error)
                  return;
                chrome.test.assertEq(
                    TESTING_TEXT_TO_WRITE,
                    fileContents['/' + TESTING_TIRAMISU_FILE_NAME]);
                onTestSuccess();
              };
              fileWriter.onerror = function(e) {
                chrome.test.fail(fileWriter.error.name);
              };
              var blob = new Blob(
                  [TESTING_TEXT_TO_WRITE], {type: 'text/plain'});
              fileWriter.write(blob);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Append contents to an existing file. It should succeed.
    function appendFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE_NAME,
          {create: false, exclusive: false},
          function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.seek(TESTING_TEXT_TO_WRITE.length);
              fileWriter.onwriteend = function(e) {
                if (fileWriter.error)
                  return;
                chrome.test.assertEq(
                    TESTING_TEXT_TO_WRITE + TESTING_TEXT_TO_WRITE,
                    fileContents['/' + TESTING_TIRAMISU_FILE_NAME]);
                onTestSuccess();
              };
              fileWriter.onerror = function(e) {
                chrome.test.fail(fileWriter.error.name);
              };
              var blob = new Blob(
                  [TESTING_TEXT_TO_WRITE], {type: 'text/plain'});
              fileWriter.write(blob);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Replace contents in an existing file. It should succeed.
    function replaceFileSuccess() {
      var onTestSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE_NAME,
          {create: false, exclusive: false},
          function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.seek(TESTING_TEXT_TO_WRITE.indexOf('creams'));
              fileWriter.onwriteend = function(e) {
                if (fileWriter.error)
                  return;
                var expectedContents = TESTING_TEXT_TO_WRITE.replace(
                    'creams', 'skates') + TESTING_TEXT_TO_WRITE;
                chrome.test.assertEq(
                    expectedContents,
                    fileContents['/' + TESTING_TIRAMISU_FILE_NAME]);
                onTestSuccess();
              };
              fileWriter.onerror = function(e) {
                chrome.test.fail(fileWriter.error.name);
              };
              var blob = new Blob(['skates'], {type: 'text/plain'});
              fileWriter.write(blob);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Write bytes to a broken file. This should result in an error.
    function writeBrokenFileError() {
      var onTestSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_BROKEN_TIRAMISU_FILE_NAME,
          {create: false, exclusive: false},
          function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.onwriteend = function(e) {
                if (fileWriter.error)
                  return;
                chrome.test.fail(
                    'Unexpectedly succeeded to write to a broken file.');
              };
              fileWriter.onerror = function(e) {
                chrome.test.assertEq(
                    'InvalidStateError', fileWriter.error.name);
                onTestSuccess();
              };
              var blob = new Blob(['A lot of flowers.'], {type: 'text/plain'});
              fileWriter.write(blob);
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
