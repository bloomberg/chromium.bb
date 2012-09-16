// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var mediaFileSystemsDirectoryEntryCallback = function(entries) {
  chrome.test.fail("Shouldn't have been able to get a directory listing.");
};

var mediaFileSystemsListCallback = function(results) {
  // There should be "Music", "Pictures", and "Videos" directories on all
  // desktop platforms.
  var expectedFileSystems = 3;
  // But not on Android and ChromeOS.
  if (/Android/.test(navigator.userAgent) || /CrOS/.test(navigator.userAgent))
    expectedFileSystems = 0;
  chrome.test.assertEq(expectedFileSystems, results.length);
  if (expectedFileSystems) {
    var dir_reader = results[0].root.createReader();
    dir_reader.readEntries(mediaFileSystemsDirectoryEntryCallback,
                           chrome.test.callbackPass());
  }
};

chrome.test.runTests([
  function getGalleries() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },
]);
