// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var galleries;
var testResults = [];

function checkFinished() {
  if (testResults.length != galleries.length)
    return;
  var success = true;
  for (var i = 0; i < testResults.length; i++) {
    if (testResults[i]) {
      success = false;
    }
  }
  if (success) {
    chrome.test.succeed();
    return;
  }
  chrome.test.fail(testResults);
}

var mediaFileSystemsDirectoryEntryCallback = function(entries) {
  testResults.push("");
  checkFinished();
}

var mediaFileSystemsDirectoryErrorCallback = function(err) {
  testResults.push("Couldn't read from directory: " + err);
  checkFinished();
};

function testGalleries(expectedFileSystems) {
  chrome.test.assertEq(expectedFileSystems, galleries.length);
  if (expectedFileSystems == 0) {
    chrome.test.succeed();
    return;
  }

  for (var i = 0; i < galleries.length; i++) {
    var dirReader = galleries[i].root.createReader();
    dirReader.readEntries(mediaFileSystemsDirectoryEntryCallback,
                          mediaFileSystemsDirectoryErrorCallback);
  }
};

var mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.runTests([
  function mediaGalleriesReadAccess() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },
]);
