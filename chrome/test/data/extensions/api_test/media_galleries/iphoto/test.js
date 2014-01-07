// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;
var expectedGalleryEntryLength; // Size of ../common/test.jpg.

//Verifies a directory itself, then the contents.
function getAndVerifyDirectoryEntry(parentEntry, directoryName,
                                    verifyFunction) {
  function getDirectoryCallback(entry) {
    chrome.test.assertTrue(entry.isDirectory);
    verifyDirectoryEntry(entry, verifyFunction);
  }

  parentEntry.getDirectory(directoryName, {create: false},
                           getDirectoryCallback, chrome.test.fail);
}

function verifyAllJPEGs(parentDirectoryEntry, filenames, doneCallback) {
  var remaining = filenames;
  function verifyNextJPEG() {
    if (remaining.length == 0) {
      doneCallback();
      return;
    }
    verifyJPEG(parentDirectoryEntry, remaining.pop(),
               expectedGalleryEntryLength, verifyNextJPEG);
  }
  verifyNextJPEG();
}

function GalleryPropertiesTest(iphotoGallery) {
  var galleryProperties =
      mediaGalleries.getMediaFileSystemMetadata(iphotoGallery);
  chrome.test.assertFalse(galleryProperties.isRemovable);
  chrome.test.succeed();
}

function RootListingTest(iphotoGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(1, entries.length);
    chrome.test.assertTrue(entries[0].isDirectory);
    chrome.test.assertEq("Albums", entries[0].name);
    chrome.test.succeed();
  }

  verifyDirectoryEntry(iphotoGallery.root, verify);
}

function AlbumsListingTest(iphotoGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isDirectory);
    chrome.test.assertTrue(entries[1].isDirectory);
    chrome.test.assertEq("Album1", entries[0].name);
    chrome.test.assertEq("Album2", entries[1].name);
    chrome.test.succeed();
  }

  getAndVerifyDirectoryEntry(iphotoGallery.root, "Albums", verify);
}

function Album1ListingTest(iphotoGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertTrue(entries[1].isFile);
    chrome.test.assertEq("InBoth.jpg", entries[0].name);
    chrome.test.assertEq("InFirstAlbumOnly.jpg", entries[1].name);

    verifyAllJPEGs(directoryEntry, ["InBoth.jpg", "InFirstAlbumOnly.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(iphotoGallery.root,
                             "Albums/Album1", verify);
}

function Album2ListingTest(iphotoGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(1, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertEq("InBoth.jpg", entries[0].name);

    verifyAllJPEGs(directoryEntry, ["InBoth.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(iphotoGallery.root,
                             "Albums/Album2", verify);
}

function getTest(testFunction) {
  function getMediaFileSystemsList() {
    mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
  }

  function getMediaFileSystemsCallback(results) {
    for (var i = 0; i < results.length; ++i) {
      var properties = mediaGalleries.getMediaFileSystemMetadata(results[i]);
      if (properties.name == "iPhoto") {
        testFunction(results[i]);
        return;
      }
    }
    chrome.test.fail("iPhoto gallery not found");
  }

  return function() {
    getMediaFileSystemsList();
  }
}

CreateDummyWindowToPreventSleep();

chrome.test.getConfig(function(config) {
  customArg = JSON.parse(config.customArg);
  expectedGalleryEntryLength = customArg[0];

  chrome.test.runTests([
    getTest(GalleryPropertiesTest),
    getTest(RootListingTest),
    getTest(AlbumsListingTest),
    getTest(Album1ListingTest),
    getTest(Album2ListingTest),
  ]);
});
