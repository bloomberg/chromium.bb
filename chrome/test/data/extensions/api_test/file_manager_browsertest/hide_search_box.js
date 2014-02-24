// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests hiding the search box.
 */
testcase.hideSearchBox = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Resize the window.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      callRemoteTestUtil('resizeWindow', appId, [100, 100], this.next);
    },
    // Wait for the style change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForStyles',
                         appId,
                         [{
                            query: '#search-box',
                            styles: {visibility: 'hidden'}
                          }],
                         this.next);
    },
    // Check the styles
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
