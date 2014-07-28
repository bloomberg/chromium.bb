// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'kitty',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_NEW_FILE = Object.freeze({
  isDirectory: false,
  name: 'puppy',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * Creates a file.
 *
 * @param {CreateFileRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
function onCreateFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId != test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.filePath == '/') {
    onError('INVALID_OPERATION');
    return;
  }

  if (options.filePath in test_util.defaultMetadata) {
    onError('EXISTS');
    return;
  }

  test_util.defaultMetadata[options.filePath] = TESTING_FILE;
  onSuccess();  // enum ProviderError.
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

  test_util.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;

  chrome.fileSystemProvider.onCreateFileRequested.addListener(
      onCreateFileRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Create a file which doesn't exist. Should succeed.
    function createFileSuccessSimple() {
      var onSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_NEW_FILE.name, {create: true},
          function(entry) {
            chrome.test.assertEq(TESTING_NEW_FILE.name, entry.name);
            chrome.test.assertFalse(entry.isDirectory);
            onSuccess();
          }, function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Create a file which exists, non-exclusively. Should succeed.
    function createFileOrOpenSuccess() {
      var onSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: true, exclusive: false},
          function(entry) {
            chrome.test.assertEq(TESTING_FILE.name, entry.name);
            chrome.test.assertFalse(entry.isDirectory);
            onSuccess();
          }, function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Create a file which exists, exclusively. Should fail.
    function createFileExistsError() {
      var onSuccess = chrome.test.callbackPass();
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: true, exclusive: true},
          function(entry) {
            chrome.test.fail('Created a file, but should fail.');
          }, function(error) {
            chrome.test.assertEq('InvalidModificationError', error.name);
            onSuccess();
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
