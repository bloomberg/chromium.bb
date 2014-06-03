// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests restoring the sorting order.
 */
testcase.restoreSortColumn = function() {
  var appId;
  var EXPECTED_FILES = TestEntryInfo.getExpectedRows([
    ENTRIES.world,
    ENTRIES.photos,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.beautiful
  ]);
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Sort by name.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['.table-header-cell:nth-of-type(1)'],
                         this.next);
    },
    // Check the sorted style of the header.
    function() {
      waitForElement(appId, '.table-header-sort-image-asc').then(this.next);
    },
    // Sort by name.
    function() {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['.table-header-cell:nth-of-type(1)'],
                         this.next);
    },
    // Check the sorted style of the header.
    function() {
      waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    // Check the sorted files.
    function() {
      waitForFiles(appId, EXPECTED_FILES, {orderCheck: true}).then(this.next);
    },
    // Open another window, where the sorted column should be restored.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Check the sorted style of the header.
    function(inAppId) {
      appId = inAppId;
      waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    // Check the sorted files.
    function() {
      waitForFiles(appId, EXPECTED_FILES, {orderCheck: true}).then(this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests restoring the current view (the file list or the thumbnail grid).
 */
testcase.restoreCurrentView = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Check the initial view.
    function(inAppId) {
      appId = inAppId;
      waitForElement(appId, '.thumbnail-grid[hidden]').then(this.next);
    },
    // Opens the gear menu.
    function() {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['#gear-button'],
                         this.next);
    },
    // Change the current view.
    function() {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['#thumbnail-view'],
                         this.next);
    },
    // Check the new current view.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId, '.detail-table[hidden]').then(this.next);
    },
    // Open another window, where the current view is restored.
    function() {
      openNewWindow(null, RootPath.DOWNLOADS, this.next);
    },
    // Check the current view.
    function(inAppId) {
      appId = inAppId;
      waitForElement(appId, '.detail-table[hidden]').then(this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
