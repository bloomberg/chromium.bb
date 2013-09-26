// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var galleries;
var testResults = [];
var foundGalleryWithEntry = false;
var expectedFileSystems;
var expectedGalleryEntryLength;

function checkFinished() {
  if (testResults.length != galleries.length)
    return;
  var success = true;
  for (var i = 0; i < testResults.length; i++) {
    if (testResults[i]) {
      success = false;
    }
  }
  if (!foundGalleryWithEntry) {
    testResults.push("Did not find gallery with 1 FileEntry");
    success = false;
  }
  if (success) {
    chrome.test.succeed();
    return;
  }
  chrome.test.fail(testResults);
}

var readFileCallback = function(file) {
  if (file.target.result.byteLength == expectedGalleryEntryLength) {
    testResults.push("");
  } else {
    testResults.push("File entry is the wrong size");
  }
  checkFinished();
}

var readFileFailedCallback = function(err) {
  testResults.push("Couldn't read file: " + err);
  checkFinished();
}

var createFileObjectCallback = function(file) {
  var reader = new FileReader();
  reader.onloadend = readFileCallback;
  reader.onerror = readFileFailedCallback;
  reader.readAsArrayBuffer(file);
}

var createFileObjectFailedCallback = function(err) {
  testResults.push("Couldn't create file: " + err);
  checkFinished();
}

var mediaFileSystemsDirectoryEntryCallback = function(entries) {
  if (entries.length == 0) {
    testResults.push("");
  } else if (entries.length == 1) {
    if (foundGalleryWithEntry) {
      testResults.push("Found multiple galleries with 1 FileEntry");
    } else {
      foundGalleryWithEntry = true;
      entries[0].file(createFileObjectCallback, createFileObjectFailedCallback);
    }
  } else {
    testResults.push("Found a gallery with more than 1 FileEntry");
  }
  checkFinished();
}

var mediaFileSystemsDirectoryErrorCallback = function(err) {
  testResults.push("Couldn't read from directory: " + err);
  checkFinished();
};

var mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.getConfig(function(config) {
  customArg = JSON.parse(config.customArg);
  expectedFileSystems = customArg[0];
  expectedGalleryEntryLength = customArg[1];

  chrome.test.runTests([
    function getMediaFileSystems() {
      mediaGalleries.getMediaFileSystems(
          chrome.test.callbackPass(mediaFileSystemsListCallback));
    },
    function readFileSystems() {
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
    },
  ]);
})
