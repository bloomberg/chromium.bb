// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

function MediaScanTest() {
  var scanProgress = 'start';
  var initialGalleryCount = 0;

  function OnScanResultsAdded(galleries) {
    chrome.test.assertEq(initialGalleryCount + 1, galleries.length);
    chrome.test.succeed();
  }

  function OnScanProgress(details) {
    chrome.test.assertEq(scanProgress, details.type);
    if (scanProgress == 'start') {
      scanProgress = 'finish';
    } else {
      scanProgress = 'done';
      chrome.test.runWithUserGesture(function() {
          mediaGalleries.addScanResults(OnScanResultsAdded);
      });
    }
  }

  function OnInitialMediaGalleries(galleries) {
    initialGalleryCount = galleries.length;
    mediaGalleries.onScanProgress.addListener(OnScanProgress);
    mediaGalleries.startMediaScan();
  }

  mediaGalleries.getMediaFileSystems(OnInitialMediaGalleries);
}

CreateDummyWindowToPreventSleep();

chrome.test.runTests([
  MediaScanTest,
]);
