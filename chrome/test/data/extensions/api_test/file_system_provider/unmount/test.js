// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string}
 * @const
 */
var FIRST_FILE_SYSTEM_ID = 'vanilla';

/**
 * @type {string}
 * @const
 */
var SECOND_FILE_SYSTEM_ID = 'ice-cream';

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  Promise.race([
    new Promise(function(fulfill, reject) {
      chrome.fileSystemProvider.mount(
          {fileSystemId: FIRST_FILE_SYSTEM_ID, displayName: 'vanilla.zip'},
          function() { fulfill(); },
          function(error) { reject(error); });
    }),
    new Promise(function(fulfill, reject) {
      chrome.fileSystemProvider.mount(
          {fileSystemId: SECOND_FILE_SYSTEM_ID, displayName: 'ice-cream.zip'},
          function() { fulfill(); },
          function(error) { reject(error); });
    })
  ]).then(callback).catch(function(error) {
    chrome.test.fail(error.name);
  });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Tests the fileSystemProvider.unmount(). Verifies if the unmount event
    // is emitted by VolumeManager.
    function unmount() {
      var onTestSuccess = chrome.test.callbackPass();

      var onMountCompleted = function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('success', event.status);
        chrome.test.assertEq(
            chrome.runtime.id, event.volumeMetadata.extensionId);
        chrome.test.assertEq(
            FIRST_FILE_SYSTEM_ID, event.volumeMetadata.fileSystemId);
        chrome.fileBrowserPrivate.onMountCompleted.removeListener(
            onMountCompleted);
        onTestSuccess();
      };

      chrome.fileBrowserPrivate.onMountCompleted.addListener(
          onMountCompleted);
      chrome.fileSystemProvider.unmount(
          {fileSystemId: FIRST_FILE_SYSTEM_ID},
          function() {
            // Wait for the unmount event.
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tests the fileSystemProvider.unmount() with a wrong id. Verifies that
    // it fails with a correct error code.
    function unmountWrongId() {
      var onTestSuccess = chrome.test.callbackPass();
      chrome.fileSystemProvider.unmount(
          {fileSystemId: 'wrong-fs-id'},
          function() {
            chrome.test.fail();
          },
          function(error) {
            chrome.test.assertEq('SecurityError', error.name);
            onTestSuccess();
          });
    },

    // Tests if fileBrowserPrivate.removeMount() for provided file systems emits
    // the onMountRequested() event with correct arguments.
    function requestUnmountSuccess() {
      var onTestSuccess = chrome.test.callbackPass();

      var onUnmountRequested = function(options, onSuccess, onError) {
        chrome.test.assertEq(SECOND_FILE_SYSTEM_ID, options.fileSystemId);
        onSuccess();
        // Not calling fileSystemProvider.unmount(), so the onMountCompleted
        // event will not be raised.
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
        onTestSuccess();
      };

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);

      test_util.getVolumeInfo(SECOND_FILE_SYSTEM_ID, function(volumeInfo) {
        chrome.test.assertTrue(!!volumeInfo);
        chrome.fileBrowserPrivate.removeMount(volumeInfo.volumeId);
      });
    },

    // End to end test with a failure. Invokes fileSystemProvider.removeMount()
    // on a provided file system, and verifies (1) if the onMountRequested()
    // event is called with correct aguments, and (2) if calling onError(),
    // results in an unmount event fired from the VolumeManager instance.
    function requestUnmountError() {
      var onTestSuccess = chrome.test.callbackPass();
      var unmountRequested = false;

      var onUnmountRequested = function(options, onSuccess, onError) {
        chrome.test.assertEq(false, unmountRequested);
        chrome.test.assertEq(SECOND_FILE_SYSTEM_ID, options.fileSystemId);
        onError('IN_USE');  // enum ProviderError.
        unmountRequested = true;
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
      };

      var onMountCompleted = function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('error_unknown', event.status);
        chrome.test.assertEq(
            chrome.runtime.id, event.volumeMetadata.extensionId);
        chrome.test.assertEq(
            SECOND_FILE_SYSTEM_ID, event.volumeMetadata.fileSystemId);
        chrome.test.assertTrue(unmountRequested);

        // Remove the handlers and mark the test as succeeded.
        chrome.fileBrowserPrivate.removeMount(SECOND_FILE_SYSTEM_ID);
        chrome.fileBrowserPrivate.onMountCompleted.removeListener(
            onMountCompleted);
        onTestSuccess();
      };

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);
      chrome.fileBrowserPrivate.onMountCompleted.addListener(onMountCompleted);

      test_util.getVolumeInfo(SECOND_FILE_SYSTEM_ID, function(volumeInfo) {
        chrome.test.assertTrue(!!volumeInfo);
        chrome.fileBrowserPrivate.removeMount(volumeInfo.volumeId);
      });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
