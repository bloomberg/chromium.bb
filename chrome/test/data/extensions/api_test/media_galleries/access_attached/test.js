// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var galleries;
var testResults = [];

function testGalleries(expectedFileSystems, testGalleryName) {
  chrome.test.assertEq(expectedFileSystems, galleries.length);

  for (var i = 0; i < galleries.length; i++) {
    var metadata = mediaGalleries.getMediaFileSystemMetadata(galleries[i]);
    if (metadata.name == testGalleryName) {
      chrome.test.succeed();
      return;
    } else {
      testResults.push(metadata.name);
    }
  }
  chrome.test.fail(testResults);
};

var mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.runTests([
  function mediaGalleriesAccessAttached() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },
]);
