// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Selects the first item in the file list.
 * @param {string} windowId ID of the target window.
 * @return {Promise} Promise to be fulfilled on success.
 */
function selectFirstListItem(windowId) {
  return Promise.resolve().then(function() {
    // Ensure no selected item.
    return waitForElementLost(windowId,
                              'div.detail-table > list > li[selected]');
  }).then(function() {
    // Push Down.
    return callRemoteTestUtil('fakeKeyDown',
                              windowId,
                              // Down
                              ['#file-list', 'Down', true]);
  }).then(function() {
    // Wait for selection.
    return waitForElement(windowId, 'div.detail-table > list > li[selected]');
  }).then(function() {
    // Ensure that only the first item is selected.
    return callRemoteTestUtil(
        'queryAllElements',
        windowId,
        ['div.detail-table > list > li[selected]']);
  }).then(function(elements) {
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq('detail-table-1', elements[0].attributes['id']);
  });
}

/**
 * Creates new folder.
 * @param {string} windowId ID of the target window.
 * @param {string} path Initial path.
 * @param {Array.<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @return {Promise} Promise to be fulfilled on success.
 */
function createNewFolder(windowId, path, initialEntrySet) {
  var newFoloderListItemId = null;
  return Promise.resolve(
  ).then(function() {
    // Push Ctrl + E.
    return callRemoteTestUtil('fakeKeyDown',
                              windowId,
                              // Ctrl + E
                              ['#file-list', 'U+0045', true]);
  }).then(function() {
    // Wait for rename text field.
    return waitForElement(windowId, 'li[renaming] input.rename');
  }).then(function() {
    return callRemoteTestUtil(
        'queryAllElements',
        windowId,
        ['div.detail-table > list > li[selected]']);
  }).then(function(elements) {
    // Ensure that only the new directory is selected and being renamed.
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertTrue('renaming' in elements[0].attributes);
    newFoloderListItemId = elements[0].attributes['id'];
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
    // Wait until rename completes.
    return waitForElementLost(windowId, 'input.rename');
  }).then(function() {
    var expectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
    expectedEntryRows.push(['Test Folder Name', '--', 'Folder', '']);
    // Wait for the new folder.
    return waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  }).then(function() {
    // Wait until the new created folder is selected.
    return waitForElement(
        windowId,
        'div.detail-table > list > li[selected]#' + newFoloderListItemId);
  }).then(function() {
    // Ensure that the created directory is the only one selected directory.
    return callRemoteTestUtil(
        'queryAllElements',
        windowId,
        ['div.detail-table > list > li[selected]']);
  }).then(function(elements) {
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq(newFoloderListItemId, elements[0].attributes['id']);
  });
};

testcase.createNewFolderAfterSelectFile = function() {
  var PATH = RootPath.DOWNLOADS;
  var windowId = null;
  var promise = new Promise(function(callback) {
    setupAndWaitUntilReady(null, PATH, callback);
  }).then(function(inWindowId) {
    windowId = inWindowId;
    return selectFirstListItem(windowId);
  }).then(function() {
    return createNewFolder(windowId, PATH, BASIC_LOCAL_ENTRY_SET);
  });

  testPromise(promise);
};

testcase.createNewFolderDownloads = function() {
  var PATH = RootPath.DOWNLOADS;
  var promise = new Promise(function(callback) {
    setupAndWaitUntilReady(null, PATH, callback);
  }).then(function(windowId) {
    return createNewFolder(windowId, PATH, BASIC_LOCAL_ENTRY_SET);
  });

  testPromise(promise);
};

testcase.createNewFolderDrive = function() {
  var PATH = RootPath.DRIVE;
  var promise = new Promise(function(callback) {
    setupAndWaitUntilReady(null, PATH, callback);
  }).then(function(windowId) {
    return createNewFolder(windowId, PATH, BASIC_DRIVE_ENTRY_SET);
  });

  testPromise(promise);
};
