// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.experimental.mediaGalleries;
var emptyListCallback = function(results) {
  chrome.test.assertEq(results, []);
};
var nullCallback = function(results) {
  chrome.test.assertEq(results, null);
};

chrome.test.runTests([
  function getGalleries() {
    mediaGalleries.getMediaGalleries(
        chrome.test.callbackPass(emptyListCallback));
  },

  function assembleMediaFile() {
    mediaGalleries.assembleMediaFile(
        {}, {}, chrome.test.callbackPass(nullCallback));
  },

  function parseMediaFileMetadata() {
    mediaGalleries.parseMediaFileMetadata(
        {}, chrome.test.callbackPass(nullCallback));
  },
]);
