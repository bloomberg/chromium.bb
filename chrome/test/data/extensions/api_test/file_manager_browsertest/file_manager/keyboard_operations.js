// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * Renames a file.
 * @param {string} windowId ID of the window.
 * @param {string} oldName Old name of a file.
 * @param {string} newName New name of a file.
 * @return {Promise} Promise to be fulfilled on success.
 */
function renameFile(windowId, oldName, newName) {
  return callRemoteTestUtil('selectFile', windowId, [oldName]).then(function() {
    // Push Ctrl+Enter.
    return fakeKeyDown(windowId, '#detail-table', 'Enter', true);
  }).then(function() {
    // Wait for rename text field.
    return waitForElement(windowId, 'input.rename');
  }).then(function() {
    // Type new file name.
    return callRemoteTestUtil('inputText', windowId, ['input.rename', newName]);
  }).then(function() {
    // Push Enter.
    return fakeKeyDown(windowId, 'input.rename', 'Enter', false);
  });
}

/**
 * Test for renaming a file.
 * @param {string} path Initial path.
 * @param {Array.<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameFile(path, initialEntrySet) {
  var windowId;

  // Make expected rows.
  var initialExpectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
  var expectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
  for (var i = 0; i < expectedEntryRows.length; i++) {
    if (expectedEntryRows[i][0] === 'hello.txt') {
      expectedEntryRows[i][0] = 'New File Name.txt';
      break;
    }
  }
  chrome.test.assertTrue(
      i != expectedEntryRows.length, 'hello.txt is not found.');

  // Open a window.
  return new Promise(function(callback) {
    setupAndWaitUntilReady(null, path, callback);
  }).then(function(inWindowId) {
    windowId = inWindowId;
    return waitForFiles(windowId, initialExpectedEntryRows);
  }).then(function(){
    return renameFile(windowId, 'hello.txt', 'New File Name.txt');
  }).then(function() {
    // Wait until rename completes.
    return waitForElementLost(windowId, '#detail-table [renaming]');
  }).then(function() {
    // Wait for the new file name.
    return waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  }).then(function() {
    return renameFile(windowId, 'New File Name.txt', '.hidden file');
  }).then(function() {
    // The error dialog is shown.
    return waitAndAcceptDialog(windowId);
  }).then(function() {
    // The name did not change.
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

testcase.createNewFolderAndCheckFocus = function() {
};

testcase.renameFileDownloads = function() {
  testPromise(testRenameFile(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET));
};

testcase.renameFileDrive = function() {
  testPromise(testRenameFile(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET));
};
