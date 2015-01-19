// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs all of the test cases, one by one.
 */
chrome.test.runTests([
  // Verifies if getAll() returns the mounted file system.
  function mountSuccess() {
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: test_util.FILE_SYSTEM_ID,
          displayName: test_util.FILE_SYSTEM_NAME,
          writable: true,
          openedFilesLimit: 2
        },
        chrome.test.callbackPass(function() {
          chrome.fileSystemProvider.getAll(chrome.test.callbackPass(
              function(fileSystems) {
                chrome.test.assertEq(1, fileSystems.length);
                chrome.test.assertEq(
                    test_util.FILE_SYSTEM_ID, fileSystems[0].fileSystemId);
                chrome.test.assertEq(
                    test_util.FILE_SYSTEM_NAME, fileSystems[0].displayName);
                chrome.test.assertTrue(fileSystems[0].writable);
                chrome.test.assertEq(2, fileSystems[0].openedFilesLimit);
              }));
          chrome.fileSystemProvider.get(
              test_util.FILE_SYSTEM_ID,
              chrome.test.callbackPass(function(fileSystem) {
                chrome.test.assertEq(
                    test_util.FILE_SYSTEM_ID, fileSystem.fileSystemId);
                chrome.test.assertEq(
                    test_util.FILE_SYSTEM_NAME, fileSystem.displayName);
                chrome.test.assertTrue(fileSystem.writable);
                chrome.test.assertEq(2, fileSystem.openedFilesLimit);
              }));
        }));
  },

  // Verifies that after unmounting, the file system is not available in
  // getAll() list.
  function unmountSuccess() {
    chrome.fileSystemProvider.unmount(
        {fileSystemId: test_util.FILE_SYSTEM_ID},
        chrome.test.callbackPass(function() {
          chrome.fileSystemProvider.getAll(chrome.test.callbackPass(
              function(fileSystems) {
                chrome.test.assertEq(0, fileSystems.length);
              }));
          chrome.fileSystemProvider.get(
              test_util.FILE_SYSTEM_ID,
              chrome.test.callbackFail('NOT_FOUND'));
        }));
  },

  // Verifies that if mounting fails, then the file system is not added to the
  // getAll() list.
  function mountError() {
    chrome.fileSystemProvider.mount(
        {fileSystemId: '', displayName: ''},
        chrome.test.callbackFail('INVALID_OPERATION', function() {
          chrome.fileSystemProvider.getAll(chrome.test.callbackPass(
              function(fileSystems) {
                chrome.test.assertEq(0, fileSystems.length);
              }));
          chrome.fileSystemProvider.get(
              test_util.FILE_SYSTEM_ID,
              chrome.test.callbackFail('NOT_FOUND'));
        }));
  }
]);
