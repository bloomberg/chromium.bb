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
      repeatUntil(function() {
        return callRemoteTestUtil(
            'queryAllElements',
            appId,
            ['#search-box', null, ['visibility']]).
            then(function(elements) {
              if (elements[0].styles.visibility !== 'hidden')
                return pending('The search box to be hidden, but it is: %j.',
                               elements[0]);
            });
      }).
      then(this.next);
    },
    // Check the styles
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
