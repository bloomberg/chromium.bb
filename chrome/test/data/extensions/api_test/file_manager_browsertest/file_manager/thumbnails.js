// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if a thumbnail for the selected item shows up in the preview panel.
 * This thumbnail is fetched via the image loader.
 */
testcase.thumbnailsDownloads = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Select the image.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('selectFile',
                         appId,
                         ['My Desktop Background.png'],
                         this.next);
    },
    // Wait until the thumbnail shows up.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId, '.preview-thumbnails .img-container img').
          then(this.next);
    },
    // Verify the thumbnail.
    function(element) {
      chrome.test.assertTrue(element.attributes.src.indexOf(
          'data:image/jpeg') === 0);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
