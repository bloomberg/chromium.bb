// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

function StartAndCancelMediaScanTest() {
  function StartMediaScanTest() {
    var startEventListener = function(details) {
      chrome.test.assertEq('start', details.type);
      mediaGalleries.onScanProgress.removeListener(startEventListener);
      CancelMediaScanTest();
    }
    mediaGalleries.onScanProgress.addListener(startEventListener);

    mediaGalleries.startMediaScan();
  }

  function CancelMediaScanTest() {
    var cancelEventListener = function(details) {
      chrome.test.assertEq('cancel', details.type);
      mediaGalleries.onScanProgress.removeListener(cancelEventListener);
      chrome.test.succeed();
    };
    mediaGalleries.onScanProgress.addListener(cancelEventListener);

    mediaGalleries.cancelMediaScan();
  }

  StartMediaScanTest();
}

CreateDummyWindowToPreventSleep();

chrome.test.runTests([
  StartAndCancelMediaScanTest,
]);
