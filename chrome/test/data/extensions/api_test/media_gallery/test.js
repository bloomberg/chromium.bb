// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.experimental.mediaGalleries;
var mediaFileSystemsListCallback = function(results) {
  // There should be a "Pictures" directory on all desktop platforms.
  chrome.test.assertEq(1, results.length);
};
var nullCallback = function(result) {
  chrome.test.assertEq(null, result);
};

chrome.test.runTests([
  function getGalleries() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },

  function openMediaGalleryManager() {
    // Just confirm that the function exists.
    chrome.test.assertTrue(mediaGalleries.openMediaGalleryManager !== null);
    chrome.test.succeed()
  },

  function extractEmbeddedThumbnails() {
    var result = mediaGalleries.extractEmbeddedThumbnails({});
    chrome.test.assertEq(null, result);
    chrome.test.succeed()
  },

  function assembleMediaFile() {
    mediaGalleries.assembleMediaFile(
        {}, {}, chrome.test.callbackPass(nullCallback));
  },
]);
