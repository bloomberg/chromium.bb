// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs all of the test cases, one by one.
 */
chrome.test.runTests([
  // Tests whether mounting succeeds, when a non-empty name is provided.
  function goodDisplayName() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.mount(
        {fileSystemId: 'file-system-id', displayName: 'file-system-name'},
        function() {
          onTestSuccess();
        },
        function(error) {
          chrome.test.fail();
        });
  },

  // Verifies that mounting fails, when an empty string is provided as a name.
  function emptyDisplayName() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.mount(
        {fileSystemId: 'file-system-id', displayName: ''},
        function() {
          chrome.test.fail();
        },
        function(error) {
          chrome.test.assertEq('SecurityError', error.name);
          onTestSuccess();
        });
  },

  // Verifies that mounting fails, when an empty string is provided as an Id
  function emptyFileSystemId() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.mount(
        {fileSystemId: '', displayName: 'File System Name'},
        function() {
          chrome.test.fail();
        },
        function(error) {
          chrome.test.assertEq('SecurityError', error.name);
          onTestSuccess();
        }
      );
  },

  // End to end test. Mounts a volume using fileSystemProvider.mount(), then
  // checks if the mounted volume is added to VolumeManager, by querying
  // fileBrowserPrivate.getVolumeMetadataList().
  function successfulMount() {
    var onTestSuccess = chrome.test.callbackPass();
    var fileSystemId = 'caramel-candy';
    chrome.fileSystemProvider.mount(
        {fileSystemId: fileSystemId, displayName: 'caramel-candy.zip'},
        function() {
          chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeList) {
            var volumeInfo;
            volumeList.forEach(function(inVolumeInfo) {
              if (inVolumeInfo.extensionId == chrome.runtime.id &&
                  inVolumeInfo.fileSystemId == fileSystemId) {
                volumeInfo = inVolumeInfo;
              }
            });
            chrome.test.assertTrue(!!volumeInfo);
            chrome.test.assertTrue(volumeInfo.isReadOnly);
            onTestSuccess();
          });
        },
        function(error) {
          chrome.test.fail();
        });
  },

  // Checks whether mounting a file system in writable mode ends up on filling
  // out the volume info properly.
  function successfulWritableMount() {
    var onTestSuccess = chrome.test.callbackPass();
    var fileSystemId = 'caramel-fudges';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: fileSystemId,
          displayName: 'caramel-fudges.zip',
          writable: true
        },
        function() {
          chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeList) {
            var volumeInfo;
            volumeList.forEach(function(inVolumeInfo) {
              if (inVolumeInfo.extensionId == chrome.runtime.id &&
                  inVolumeInfo.fileSystemId == fileSystemId) {
                volumeInfo = inVolumeInfo;
              }
            });
            chrome.test.assertTrue(!!volumeInfo);
            chrome.test.assertFalse(volumeInfo.isReadOnly);
            onTestSuccess();
          });
        },
        function(error) {
          chrome.test.fail();
        });
  },

  // Checks is limit for mounted file systems per profile works correctly.
  // Tries to create more than allowed number of file systems. All of the mount
  // requests should succeed, except the last one which should fail with a
  // security error.
  function stressMountTest() {
    var onTestSuccess = chrome.test.callbackPass();
    var ALREADY_MOUNTED_FILE_SYSTEMS = 3;  // By previous tests.
    var MAX_FILE_SYSTEMS = 16;
    var index = 0;
    var tryNextOne = function() {
      index++;
      if (index < MAX_FILE_SYSTEMS - ALREADY_MOUNTED_FILE_SYSTEMS + 1) {
        var fileSystemId = index + '-stress-test';
        chrome.fileSystemProvider.mount(
            {fileSystemId: fileSystemId, displayName: index + 'th File System'},
            function() {
              tryNextOne();
            },
            function(error) {
              chrome.test.fail(error.name);
            });
      } else {
        chrome.fileSystemProvider.mount(
            {
              fileSystemId: 'over-the-limit-fs-id',
              displayName: 'Over The Limit File System'
            },
            function() {
              chrome.test.fail();
            },
            function(error) {
              chrome.test.assertEq('SecurityError', error.name);
              onTestSuccess();
            });
      }
    };
    tryNextOne();
  }
]);
