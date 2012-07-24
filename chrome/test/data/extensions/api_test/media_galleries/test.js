// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.experimental.app.onLaunched.addListener(function() {
  var experimentalMediaGalleries = chrome.experimental.mediaGalleries;
  var mediaGalleries = chrome.mediaGalleries;
  var mediaFileSystemsListCallback = function(results) {
    // There should be a "Pictures" directory on all desktop platforms.
    var expectedFileSystems = 1;
    // But not on Android and ChromeOS.
    if (/Android/.test(navigator.userAgent) || /CrOS/.test(navigator.userAgent))
      expectedFileSystems = 0;
    chrome.test.assertEq(expectedFileSystems, results.length);
    // TODO(vandebo) Test that we can read from the file system object.
  };
  var nullCallback = function(result) {
    chrome.test.assertEq(null, result);
  };

  function runTests(tests) {
    chrome.test.getConfig(function(config) {
      operatingSystem = config.osName;
      chrome.test.runTests(tests);
    });
  }

  chrome.test.runTests([
    function getGalleries() {
      mediaGalleries.getMediaFileSystems(
          chrome.test.callbackPass(mediaFileSystemsListCallback));
    },

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
});
