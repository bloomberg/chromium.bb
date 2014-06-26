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
 * @type {string}
 * @const
 */
var TESTING_MIME_TYPE = 'text/secret-testing-mime-type';

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
var TESTING_WITH_MIME_FILE = Object.freeze({
  isDirectory: false,
  name: 'first-file.abc',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  mimeType: TESTING_MIME_TYPE
});

/**
 * @type {Object}
 * @const
 */
var TESTING_WITHOUT_MIME_FILE = Object.freeze({
  isDirectory: false,
  name: 'second-file.abc',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
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
 * @param {GetMetadataRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(options, onSuccess, onError) {
  if (options.fileSystemId != FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (options.entryPath == '/' + TESTING_WITH_MIME_FILE.name) {
    onSuccess(TESTING_WITH_MIME_FILE);
    return;
  }

  if (options.entryPath == '/' + TESTING_WITHOUT_MIME_FILE.name) {
    onSuccess(TESTING_WITHOUT_MIME_FILE);
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
  chrome.fileSystemProvider.mount(
      {fileSystemId: FILE_SYSTEM_ID, displayName: 'chocolate.zip'},
      function() {
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
      },
      function() {
        chrome.test.fail();
      });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Test if the file with a mime type handled by this testing extension
    // appears on a task list.
    function withMimeIsTask() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(TESTING_WITH_MIME_FILE.name, {}, function(entry) {
        chrome.fileBrowserPrivate.getFileTasks(
            [entry.toURL()],
            // TODO(mtomasz): Get rid of this. See: crbug.com/388586.
            [TESTING_MIME_TYPE],
            function(tasks) {
              chrome.test.assertEq(1, tasks.length);
              chrome.test.assertEq(
                  "ddammdhioacbehjngdmkjcjbnfginlla|app|magic_handler",
                  tasks[0].taskId);
              onSuccess();
            });
      }, function(error) {
        chrome.test.fail(error.name);
      });
    },
    // Confirm, that executing that task, will actually run an OnLaunched event.
    // This is another code path, than collecting tasks (tested above).
    function withMimeExecute() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(TESTING_WITH_MIME_FILE.name, {}, function(entry) {
        chrome.fileBrowserPrivate.getFileTasks(
            [entry.toURL()],
            // TODO(mtomasz): Get rid of this. See: crbug.com/388586.
            [TESTING_MIME_TYPE],
            function(tasks) {
              chrome.test.assertEq(1, tasks.length);
              chrome.test.assertEq(
                  "ddammdhioacbehjngdmkjcjbnfginlla|app|magic_handler",
                  tasks[0].taskId);
              var onLaunched = function(event) {
                chrome.test.assertTrue(!!event);
                chrome.test.assertEq("magic_handler", event.id);
                chrome.test.assertTrue(!!event.items);
                chrome.test.assertEq(1, event.items.length);
                chrome.test.assertEq(
                    TESTING_MIME_TYPE, event.items[0].type);
                chrome.test.assertEq(
                    TESTING_WITH_MIME_FILE.name,
                    event.items[0].entry.name);
                chrome.app.runtime.onLaunched.removeListener(onLaunched);
                onSuccess();
              };
              chrome.app.runtime.onLaunched.addListener(onLaunched);
              chrome.fileBrowserPrivate.executeTask(
                  tasks[0].taskId, [entry.toURL()]);
            });
      }, function(error) {
        chrome.test.fail(error.name);
      });
    },
    // The file without a mime set must not appear on the task list for this
    // testing extension.
    function withoutMime() {
      var onSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_WITHOUT_MIME_FILE.name,
          {},
          function(entry) {
            chrome.fileBrowserPrivate.getFileTasks(
                [entry.toURL()],
                // TODO(mtomasz): Get rid of this. See: crbug.com/388586.
                [],
                function(tasks) {
                  chrome.test.assertEq(0, tasks.length);
                  onSuccess();
                });
          }, function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
