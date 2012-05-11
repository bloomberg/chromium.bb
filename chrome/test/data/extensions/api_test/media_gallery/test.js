// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.experimental.mediaGalleries;
var emptyListCallback = function(results) {
  chrome.test.assertEq(results, []);
};
var nullCallback = function(result) {
  chrome.test.assertEq(result, null);
};

chrome.test.runTests([
  function getGalleries() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(emptyListCallback));
  },

  function openMediaGalleryManager() {
    // Just confirm that the function exists.
    chrome.test.assertTrue(mediaGalleries.openMediaGalleryManager !== null);
    chrome.test.succeed()
  },

  function extractEmbeddedThumbnails() {
    var result = mediaGalleries.extractEmbeddedThumbnails({});
    chrome.test.assertEq(result, null);
    chrome.test.succeed()
  },

  function assembleMediaFile() {
    mediaGalleries.assembleMediaFile(
        {}, {}, chrome.test.callbackPass(nullCallback));
  },
]);
