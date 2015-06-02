// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Verify that the configuration flag is propagated properly.
    function configureConfigurable() {
      var onConfigureRequested = chrome.test.callbackPass(
          function(options, onSuccess, onError) {
            chrome.fileSystemProvider.onConfigureRequested.removeListener(
                onConfigureRequested);
            onSuccess();
          });
      chrome.fileSystemProvider.onConfigureRequested.addListener(
          onConfigureRequested);

      chrome.fileManagerPrivate.getProvidingExtensions(
          chrome.test.callbackPass(function(extensions) {
            chrome.test.assertEq(extensions.length, 1);
            chrome.test.assertEq(chrome.runtime.id, extensions[0].extensionId);
            chrome.test.assertEq(
                chrome.runtime.getManifest().name, extensions[0].name);
            chrome.test.assertTrue(extensions[0].configurable);
            chrome.test.assertFalse(extensions[0].multipleMounts);
            chrome.test.assertEq('device', extensions[0].source);
          }));

      chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
          chrome.test.callbackPass(function() {}));
    },

    // Verify that chrome.fileManager.configureVolume is well wired
    // to onConfigureRequested().
    function configureSuccess() {
      var configured = false;
      var onConfigureRequested = chrome.test.callbackPass(
          function(options, onSuccess, onError) {
            chrome.fileSystemProvider.onConfigureRequested.removeListener(
                onConfigureRequested);
            configured = true;
            onSuccess();
          });
      chrome.fileSystemProvider.onConfigureRequested.addListener(
          onConfigureRequested);

      chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
          chrome.test.callbackPass(function() {
            chrome.test.assertTrue(configured);
          }));
    },

    // Verify that a failure is propagated properly.
    function configureFailure() {
      var onConfigureRequested = chrome.test.callbackPass(
          function(options, onSuccess, onError) {
            chrome.fileSystemProvider.onConfigureRequested.removeListener(
                onConfigureRequested);
            onError('FAILED');
          });

      chrome.fileSystemProvider.onConfigureRequested.addListener(
          onConfigureRequested);

      chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
          chrome.test.callbackFail('Failed to complete configuration.'));
    }

  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
