// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Namespace for testing utilities.
var test_util = {};

/**
 * @type {string}
 * @const
 */
test_util.FILE_SYSTEM_ID = 'vanilla.txt';

/**
 * @type {string}
 * @const
 */
test_util.FILE_SYSTEM_NAME = 'Vanilla';

/**
 * @type {FileSystem}
 */
test_util.fileSystem = null;

/**
 * Default metadata. Used by onMetadataRequestedDefault(). The key is a full
 * path, and the value, a MetadataEntry object.
 *
 * @param {Object.<string, Object>}
 */
test_util.defaultMetadata = {
  '/': {
    isDirectory: true,
    name: '',
    size: 0,
    modificationTime: new Date(2014, 4, 28, 10, 39, 15)
  }
};

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @param {function(Object)} callback Callback to be called on result, with the
 *     volume information object in case of success, or null if not found.
 */
test_util.getVolumeInfo = function(fileSystemId, callback) {
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
};

/**
 * Mounts a testing file system and calls the callback in case of a success.
 * On failure, the current test case is failed on an assertion.
 *
 * @param {function()} callback Success callback.
 */
test_util.mountFileSystem = function(callback) {
  chrome.fileSystemProvider.mount(
      {
        fileSystemId: test_util.FILE_SYSTEM_ID,
        displayName: test_util.FILE_SYSTEM_NAME
      },
      function() {
        var volumeId =
            'provided:' + chrome.runtime.id + '-' + test_util.FILE_SYSTEM_ID +
            '-user';

        test_util.getVolumeInfo(test_util.FILE_SYSTEM_ID, function(volumeInfo) {
          chrome.test.assertTrue(!!volumeInfo);
          chrome.fileBrowserPrivate.requestFileSystem(
              volumeInfo.volumeId,
              function(inFileSystem) {
                chrome.test.assertTrue(!!inFileSystem);
                test_util.fileSystem = inFileSystem;
                callback();
              });
        });
      }, function() {
        chrome.test.fail();
      });
};

/**
 * Default implementation for the metadata request event. It should be used
 * only if metadata handling is not being tested.
 *
 * @param {GetMetadataRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
test_util.onGetMetadataRequestedDefault = function(
    options, onSuccess, onError) {
  if (options.fileSystemId != test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (!(options.entryPath in test_util.defaultMetadata)) {
    onError('NOT_FOUND');
    return;
  }

  onSuccess(test_util.defaultMetadata[options.entryPath]);
};
