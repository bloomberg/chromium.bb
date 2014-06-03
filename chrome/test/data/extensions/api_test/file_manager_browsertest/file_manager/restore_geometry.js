// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests restoring geometry of the Files app.
 */
testcase.restoreGeometry = function() {
  var appId;
  var appId2;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Resize the window to minimal dimensions.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'resizeWindow', appId, [640, 480], this.next);
    },
    // Check the current window's size.
    function(inAppId) {
      waitForWindowGeometry(appId, 640, 480).then(this.next);
    },
    // Enlarge the window by 10 pixels.
    function(result) {
      callRemoteTestUtil(
          'resizeWindow', appId, [650, 490], this.next);
    },
    // Check the current window's size.
    function() {
      waitForWindowGeometry(appId, 650, 490).then(this.next);
    },
    // Open another window, where the current view is restored.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Check the next window's size.
    function(inAppId) {
      appId2 = inAppId;
      waitForWindowGeometry(appId2, 650, 490).then(this.next);
    },
    // Check for errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
