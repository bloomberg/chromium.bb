// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests copying a file to the same directory and waits until the file lists
 * changes.
 *
 * @param {string} path Directory path to be tested.
 */
function keyboardCopy(path, callback) {
  var filename = 'world.ogv';
  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();
  var expectedFilesAfter =
      expectedFilesBefore.concat([['world (1).ogv', '59 KB', 'OGG video']]);

  var appId, fileListBefore;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Copy the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertEq(expectedFilesBefore, inFileListBefore);
      callRemoteTestUtil('copyFile', appId, [filename], this.next);
    },
    // Wait for a file list change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [expectedFilesAfter,
                          {ignoreLastModifiedTime: true}],
                         this.next);
    },
    // Verify the result.
    function(fileList) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests deleting a file and and waits until the file lists changes.
 * @param {string} path Directory path to be tested.
 */
function keyboardDelete(path) {
  // Returns true if |fileList| contains |filename|.
  var isFilePresent = function(filename, fileList) {
    for (var i = 0; i < fileList.length; i++) {
      if (getFileName(fileList[i]) == filename)
        return true;
    }
    return false;
  };

  var filename = 'world.ogv';
  var directoryName = 'photos';
  var appId, fileListBefore;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Delete the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertTrue(isFilePresent(filename, fileListBefore));
      callRemoteTestUtil(
          'deleteFile', appId, [filename], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitAndAcceptDialog', appId, [], this.next);
    },
    // Wait for a file list change.
    function() {
      callRemoteTestUtil('waitForFileListChange',
                         appId,
                         [fileListBefore.length],
                         this.next);
    },
    // Delete the directory.
    function(fileList) {
      fileListBefore = fileList;
      chrome.test.assertFalse(isFilePresent(filename, fileList));
      chrome.test.assertTrue(isFilePresent(directoryName, fileList));
      callRemoteTestUtil('deleteFile', appId, [directoryName], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitAndAcceptDialog', appId, [], this.next);
    },
    // Wait for a file list change.
    function() {
      callRemoteTestUtil('waitForFileListChange', appId,
                         [fileListBefore.length], this.next);
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertFalse(isFilePresent(directoryName, fileList));
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.keyboardCopyDownloads = function() {
  keyboardCopy(RootPath.DOWNLOADS);
};

testcase.keyboardDeleteDownloads = function() {
  keyboardDelete(RootPath.DOWNLOADS);
};

testcase.keyboardCopyDrive = function() {
  keyboardCopy(RootPath.DRIVE);
};

testcase.keyboardDeleteDrive = function() {
  keyboardDelete(RootPath.DRIVE);
};
