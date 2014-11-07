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
  return remoteCall.waitForElement(windowId, '.cr-dialog-ok').
      then(remoteCall.callRemoteTestUtil.bind(remoteCall,
                                              'fakeMouseClick',
                                              windowId,
                                              ['.cr-dialog-ok'],
                                              null)).
      then(function(result) {
        chrome.test.assertTrue(result);
        return remoteCall.waitForElementLost(windowId, '.cr-dialog-container');
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
      remoteCall.callRemoteTestUtil('copyFile', appId, [filename], this.next);
    },
    // Wait for a file list change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(
          appId, expectedFilesAfter, {ignoreLastModifiedTime: true}).
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
      remoteCall.callRemoteTestUtil(
          'deleteFile', appId, [filename], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Wait for a file list change.
    function() {
      remoteCall.waitForFileListChange(appId, fileListBefore.length).
        then(this.next);
    },
    // Delete the directory.
    function(fileList) {
      fileListBefore = fileList;
      chrome.test.assertFalse(isFilePresent(filename, fileList));
      chrome.test.assertTrue(isFilePresent(directoryName, fileList));
      remoteCall.callRemoteTestUtil(
          'deleteFile', appId, [directoryName], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Wait for a file list change.
    function() {
      remoteCall.waitForFileListChange(
          appId, fileListBefore.length).then(this.next);
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
  return remoteCall.callRemoteTestUtil('selectFile', windowId, [oldName]).
    then(function() {
      // Push Ctrl+Enter.
      return remoteCall.fakeKeyDown(windowId, '#detail-table', 'Enter', true);
    }).then(function() {
      // Wait for rename text field.
      return remoteCall.waitForElement(windowId, 'input.rename');
    }).then(function() {
      // Type new file name.
      return remoteCall.callRemoteTestUtil(
          'inputText', windowId, ['input.rename', newName]);
    }).then(function() {
      // Push Enter.
      return remoteCall.fakeKeyDown(windowId, 'input.rename', 'Enter', false);
    });
}

/**
 * Test for renaming a new directory.
 * @param {string} path Initial path.
 * @param {Array.<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @param {string} pathInBreadcrumb Initial path which is shown in breadcrumb.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameNewDirectory(path, initialEntrySet, pathInBreadcrumb) {
  var expectedRows = TestEntryInfo.getExpectedRows(initialEntrySet);

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(null, path, resolve);
  }).then(function(windowId) {
    return remoteCall.waitForFiles(windowId, expectedRows).then(function() {
      return remoteCall.fakeKeyDown(windowId, '#list-container', 'U+0045',
          true);
    }).then(function() {
      // Wait for rename text field.
      return remoteCall.waitForElement(windowId, 'input.rename');
    }).then(function() {
      // Type new file name.
      return remoteCall.callRemoteTestUtil(
          'inputText', windowId, ['input.rename', 'foo']);
    }).then(function() {
      // Press Enter.
      return remoteCall.fakeKeyDown(windowId, 'input.rename', 'Enter', false);
    }).then(function() {
      // Press Enter again to try to get into the new directory.
      return remoteCall.fakeKeyDown(windowId, '#list-container', 'Enter',
          false);
    }).then(function() {
      // Confirm that it doesn't move the directory since it's in renaming
      // process.
      return remoteCall.waitUntilCurrentDirectoryIsChanged(windowId,
          pathInBreadcrumb);
    }).then(function() {
      // Wait until rename is completed.
      return remoteCall.waitForElementLost(windowId, 'li[renaming]');
    }).then(function() {
      // Press Enter again.
      return remoteCall.fakeKeyDown(windowId, '#list-container', 'Enter',
          false);
    }).then(function() {
      // Confirm that it moves to renamed directory.
      return remoteCall.waitUntilCurrentDirectoryIsChanged(windowId,
          pathInBreadcrumb + '/foo');
    });
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
    return remoteCall.waitForFiles(windowId, initialExpectedEntryRows);
  }).then(function(){
    return renameFile(windowId, 'hello.txt', 'New File Name.txt');
  }).then(function() {
    // Wait until rename completes.
    return remoteCall.waitForElementLost(windowId, '#detail-table [renaming]');
  }).then(function() {
    // Wait for the new file name.
    return remoteCall.waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  }).then(function() {
    return renameFile(windowId, 'New File Name.txt', '.hidden file');
  }).then(function() {
    // The error dialog is shown.
    return waitAndAcceptDialog(windowId);
  }).then(function() {
    // The name did not change.
    return remoteCall.waitForFiles(windowId,
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

testcase.renameNewDirectoryDownloads = function() {
  testPromise(testRenameNewDirectory(RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET, '/Downloads'));
};

testcase.renameNewDirectoryDrive = function() {
  testPromise(testRenameNewDirectory(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET,
      '/My Drive'));
};
