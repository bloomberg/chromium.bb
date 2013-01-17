// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var mediaFileSystemsGetMetadata = function(results) {
  if (/Android/.test(navigator.userAgent) || /CrOS/.test(navigator.userAgent)) {
    chrome.test.succeed();
    return;
  }
  if (results.length < 1) {
    chrome.test.fail(results);
    return;
  }

  for (var i = 0; i < results.length; i++) {
    var metadata = mediaGalleries.getMediaFileSystemMetadata(results[i]);
    if (!metadata || !metadata.name || !("galleryId" in metadata) ||
        !("isRemovable" in metadata) || !("isMediaDevice" in metadata)) {
      chrome.test.fail(metadata);
      return;
    }
  }
  chrome.test.succeed();
};

chrome.test.runTests([
  function mediaGalleriesMetadata() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsGetMetadata));
  },
]);
