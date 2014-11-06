// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_FILE = Object.freeze({
  isDirectory: true,
  name: 'tiramisu.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * List of directory changed events received from the chrome.fileManagerPrivate
 * API.
 * @type {Array.<Object>}
 */
var directoryChangedEvents = [];

/**
 * Callback to be called when a directory changed event arrives.
 * @type {function()|null}
 */
var directoryChangedCallback = null;

/**
 * Handles an event dispatched from the chrome.fileManagerPrivate API.
 * @param {Object} event Event with the directory change details.
 */
function onDirectoryChanged(event) {
  directoryChangedEvents.push(event);
  chrome.test.assertTrue(!!directoryChangedCallback);
  directoryChangedCallback();
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(onDirectoryChanged);

  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onAddWatcherRequested.addListener(
      test_util.onAddWatcherRequested);
  chrome.fileSystemProvider.onRemoveWatcherRequested.addListener(
      test_util.onRemoveWatcherRequested);

  test_util.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([

    // Adds a watcher, and then notifies that the entry has changed.
    function notifyChanged() {
      test_util.fileSystem.root.getDirectory(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry.toURL(),
                chrome.test.callbackPass(function(result) {
                  chrome.test.assertTrue(result);
                  // Verify closure called when an even arrives.
                  directoryChangedCallback = chrome.test.callbackPass(
                      function() {
                        chrome.test.assertEq(1, directoryChangedEvents.length);
                      });
                  // TODO(mtomasz): Add more advanced tests, eg. for the tag
                  // and for details of changes.
                  chrome.fileSystemProvider.notify({
                    fileSystemId: test_util.FILE_SYSTEM_ID,
                    observedPath: fileEntry.fullPath,
                    recursive: false,
                    changeType: 'CHANGED'
                  }, chrome.test.callbackPass());
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
