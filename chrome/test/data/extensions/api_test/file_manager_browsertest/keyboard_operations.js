// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits until a dialog with an OK button is shown and accepts it.
 *
 * @param {string} windowId Target window ID.
 * @return {Promise} Promise to be fulfilled after clicking the OK button in the
 *     dialog.
 */
function waitAndAcceptDialog(windowId) {
  return waitForElement(windowId, '.cr-dialog-ok').
      then(callRemoteTestUtil.bind(null,
                                   'fakeMouseClick',
                                   windowId,
                                   ['.cr-dialog-ok'],
                                   null)).
      then(function(result) {
        chrome.test.assertTrue(result);
        return waitForElementLost(windowId, '.cr-dialog-container');
      });
}

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
      waitForFiles(appId, expectedFilesAfter, {ignoreLastModifiedTime: true}).
          then(this.next);
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
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Wait for a file list change.
    function() {
      waitForFileListChange(appId, fileListBefore.length).then(this.next);
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
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Wait for a file list change.
    function() {
      waitForFileListChange(appId, fileListBefore.length).then(this.next);
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertFalse(isFilePresent(directoryName, fileList));
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise.
 */
function testPromise(promise) {
  promise.then(function() {
    return new Promise(checkIfNoErrorsOccured);
  }).then(chrome.test.callbackPass(function() {
    chrome.test.succeed();
  }), function(error) {
    chrome.test.fail(error.stack || error);
  });
};

/**
 * Test for creating new folder.
 * @param {string} path Initial path.
 * @param {Array.<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @return {Promise} Promise to be fulfilled on success.
 */
function createNewFolder(path, initialEntrySet) {
  var windowId;
  // Open window.
  return new Promise(function(callback) {
    setupAndWaitUntilReady(null, path, callback);
  }).then(function(inWindowId) {
    windowId = inWindowId;
    // Push Ctrl + E.
    return callRemoteTestUtil('fakeKeyDown',
                              windowId,
                              // Ctrl + E
                              ['#file-list', 'U+0045', true]);
  }).then(function() {
    // Wait for rename text field.
    return waitForElement(windowId, 'input.rename');
  }).then(function() {
    // Type new folder name.
    return callRemoteTestUtil(
        'inputText', windowId, ['input.rename', 'Test Folder Name']);
  }).then(function() {
    // Push Enter.
    return callRemoteTestUtil('fakeKeyDown',
                              windowId,
                              ['input.rename', 'Enter', false]);
  }).then(function() {
    return waitForElementLost(windowId, 'input.rename');
  }).then(function() {
    var expectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
    expectedEntryRows.push(['Test Folder Name', '--', 'Folder', '']);
    // Wait for the new folder.
    return waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  });
};

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

testcase.createNewFolderDownloads = function() {
  testPromise(createNewFolder(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET));
};

testcase.createNewFolderDrive = function() {
  testPromise(createNewFolder(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET));
};
