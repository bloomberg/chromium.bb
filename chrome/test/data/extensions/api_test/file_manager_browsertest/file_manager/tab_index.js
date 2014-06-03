// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests the focus behavior of the search box.
 */
testcase.searchBoxFocus = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Press the Ctrl-F key.
    function(element) {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['body', 'U+0046', true],
                         this.next);
    },
    // Check that the search box has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId, ['#search-box input:focus']).then(this.next);
    },
    // Press the Tab key.
    function(element) {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['body', 'U+0009', false],
                         this.next);
    },
    // Check that the file list has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Check for errors.
    function(element) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
