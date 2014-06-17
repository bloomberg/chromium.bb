// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Opens a file dialog and waits for closing it.
 *
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     funciton.
 * @param {Array.<TestEntryInfo>} expectedSet Expected set of the entries.
 * @param {function(windowId:string):Promise} closeDialog Function to close the
 *     dialog.
 * @return {Promise} Promise to be fulfilled with the result entry of the
 *     dialog.
 */
function openAndWaitForClosingDialog(volumeName, expectedSet, closeDialog) {
  var resultPromise = new Promise(function(fulfill) {
    chrome.fileSystem.chooseEntry(
        {type: 'openFile'},
        function(entry) { fulfill(entry); });
  });

  return waitForWindow('dialog#').then(function(windowId) {
    return waitForElement(windowId, '#file-list').
        then(function() {
          // Wait for initialization of Files.app.
          return waitForFiles(
              windowId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));
        }).
        then(function() {
          return callRemoteTestUtil('selectVolume', windowId, [volumeName]);
        }).
        then(function() {
          var expectedRows = TestEntryInfo.getExpectedRows(expectedSet);
          return waitForFiles(windowId, expectedRows);
        }).
        then(function() {
          return callRemoteTestUtil('selectFile', windowId, ['hello.txt']);
        }).
        then(closeDialog.bind(null, windowId)).
        then(function() {
          return repeatUntil(function() {
            return callRemoteTestUtil('getWindows', null, []).
                then(function(windows) {
                  if (windows[windowId])
                    return pending('Window %s does not hide.', windowId);
                  else
                    return resultPromise;
                });
          });
        });
  });
}

/**
 * Tests to open and cancel the file dialog.
 *
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     funciton.
 * @param {Array.<TestEntryInfo>} expectedSet Expected set of the entries.
 * @return {Promsie} Promise to be fulfilled/rejected depending on the test
 *     result.
 */
function openFileDialog(volumeName, expectedSet) {
  var localEntriesPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  var driveEntriesPromise = addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);
  var setupPromise = Promise.all([localEntriesPromise, driveEntriesPromise]);

  var closeByCancelButtonPromise = setupPromise.then(function() {
    return openAndWaitForClosingDialog(
        volumeName,
        expectedSet,
        function(windowId) {
          return waitForElement(windowId, '.button-panel button.cancel').then(
              function() {
                return callRemoteTestUtil(
                    'fakeEvent',
                    windowId,
                    ['.button-panel button.cancel', 'click']);
              });
        });
  }).then(function(result) {
    // Undefined means the dialog is canceled.
    chrome.test.assertEq(undefined, result);
  });

  var closeByEscKeyPromise = closeByCancelButtonPromise.then(function() {
    return openAndWaitForClosingDialog(
        volumeName,
        expectedSet,
        function(windowId) {
          return callRemoteTestUtil(
              'fakeKeyDown',
              windowId,
              ['#file-list', 'U+001B', false]);
        });
  }).then(function(result) {
    // Undefined means the dialog is canceled.
    chrome.test.assertEq(undefined, result);
  });

  var closeByOkButtonPromise = closeByEscKeyPromise.then(function() {
    return openAndWaitForClosingDialog(
        volumeName,
        expectedSet,
        function(windowId) {
          return waitForElement(windowId, '.button-panel button.ok').then(
              function() {
                return callRemoteTestUtil(
                    'fakeEvent',
                    windowId,
                    ['.button-panel button.ok', 'click']);
              });
        });
  }).then(function(result) {
    chrome.test.assertEq('hello.txt', result.name);
  });

  return closeByOkButtonPromise;
}

testcase.openFileDialogOnDownloads = function() {
  testPromise(openFileDialog('downloads', BASIC_LOCAL_ENTRY_SET));
};

testcase.openFileDialogOnDrive = function() {
  testPromise(openFileDialog('drive', BASIC_DRIVE_ENTRY_SET));
};
