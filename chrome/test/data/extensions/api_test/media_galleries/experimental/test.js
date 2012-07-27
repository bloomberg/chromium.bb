// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var experimentalMediaGalleries = chrome.experimental.mediaGalleries;

var nullCallback = function(result) {
  chrome.test.assertEq(null, result);
};

chrome.test.runTests([
  function extractEmbeddedThumbnails() {
    var result = experimentalMediaGalleries.extractEmbeddedThumbnails({});
    chrome.test.assertEq(null, result);
    chrome.test.succeed();
  },

  function assembleMediaFile() {
    experimentalMediaGalleries.assembleMediaFile(
        new Blob, {},
        chrome.test.callbackPass(nullCallback));
  },
]);
