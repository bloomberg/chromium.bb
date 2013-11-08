// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;
var expectedGalleryEntryLength = 306;  // hard-coded size of ../common/test.jpg

// Get's entire listing from directory, then verifies the sorted contents.
function verifyDirectoryEntry(directoryEntry, verifyFunction) {
  var allEntries = [];
  var reader = directoryEntry.createReader();

  function readEntries() {
    reader.readEntries(readEntriesCallback, chrome.test.fail);
  }

  function readEntriesCallback(entries) {
    if (entries.length == 0) {
      verifyFunction(directoryEntry, allEntries.sort());
      return;
    }

    allEntries = allEntries.concat(entries);
    readEntries();
  }

  readEntries();
}

// Verifies a directory itself, then the contents.
function getAndVerifyDirectoryEntry(parentEntry, directoryName,
                                    verifyFunction) {
  function getDirectoryCallback(entry) {
    chrome.test.assertTrue(entry.isDirectory);
    verifyDirectoryEntry(entry, verifyFunction);
  }

  parentEntry.getDirectory(directoryName, {create: false},
                           getDirectoryCallback, chrome.test.fail);
}

function verifyJPEG(parentDirectoryEntry, filename, doneCallback) {
  function verifyFile(file) {
    var reader = new FileReader();

    reader.onload = function(e) {
      var arraybuffer = e.target.result;
      chrome.test.assertEq(expectedGalleryEntryLength,
                           arraybuffer.byteLength);
      doneCallback();
    }

    reader.onerror =
      chrome.test.fail.bind(null, "Unable to read test image: " + filename);

    reader.readAsArrayBuffer(file);
  }

  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail)
  }

  parentDirectoryEntry.getFile(filename, {create: false}, verifyFileEntry,
                               chrome.test.fail);
}

function verifyAllJPEGs(parentDirectoryEntry, filenames, doneCallback) {
  var remaining = filenames;
  function verifyNextJPEG() {
    if (remaining.length == 0) {
      doneCallback();
      return;
    }
    verifyJPEG(parentDirectoryEntry, remaining.pop(), verifyNextJPEG);
  }
  verifyNextJPEG();
}

function GalleryPropertiesTest(picasaGallery) {
  var galleryProperties =
      mediaGalleries.getMediaFileSystemMetadata(picasaGallery);
  chrome.test.assertFalse(galleryProperties.isRemovable);
  chrome.test.succeed();
}

function RootListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isDirectory);
    chrome.test.assertTrue(entries[1].isDirectory);
    chrome.test.assertEq("albums", entries[0].name);
    chrome.test.assertEq("folders", entries[1].name);
    chrome.test.succeed();
  }

  verifyDirectoryEntry(picasaGallery.root, verify);
}

function AlbumsListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isDirectory);
    chrome.test.assertTrue(entries[1].isDirectory);
    chrome.test.assertEq("Album 1 Name 1899-12-30", entries[0].name);
    chrome.test.assertEq("Album 2 Name 1899-12-30", entries[1].name);
    chrome.test.succeed();
  }

  getAndVerifyDirectoryEntry(picasaGallery.root, "albums", verify);
}

function FoldersListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isDirectory);
    chrome.test.assertTrue(entries[1].isDirectory);
    chrome.test.assertEq("folder1 1899-12-30", entries[0].name);
    chrome.test.assertEq("folder2 1899-12-30", entries[1].name);
    chrome.test.succeed();
  }

  getAndVerifyDirectoryEntry(picasaGallery.root, "folders", verify);
}

function Album1ListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertTrue(entries[1].isFile);
    chrome.test.assertEq("InBoth.jpg", entries[0].name);
    chrome.test.assertEq("InFirstAlbumOnly.jpg", entries[1].name);

    verifyAllJPEGs(directoryEntry, ["InBoth.jpg", "InFirstAlbumOnly.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(picasaGallery.root,
                             "albums/Album 1 Name 1899-12-30", verify);
}

function Album2ListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertTrue(entries[1].isFile);
    chrome.test.assertEq("InBoth.jpg", entries[0].name);
    chrome.test.assertEq("InSecondAlbumOnly.jpg", entries[1].name);

    verifyAllJPEGs(directoryEntry, ["InBoth.jpg", "InSecondAlbumOnly.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(picasaGallery.root,
                             "albums/Album 2 Name 1899-12-30", verify);
}

function Folder1ListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(2, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertTrue(entries[1].isFile);
    chrome.test.assertEq("InBoth.jpg", entries[0].name);
    chrome.test.assertEq("InSecondAlbumOnly.jpg", entries[1].name);

    verifyAllJPEGs(directoryEntry, ["InBoth.jpg", "InSecondAlbumOnly.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(picasaGallery.root,
                             "folders/folder1 1899-12-30", verify);
}

function Folder2ListingTest(picasaGallery) {
  function verify(directoryEntry, entries) {
    chrome.test.assertEq(1, entries.length);
    chrome.test.assertTrue(entries[0].isFile);
    chrome.test.assertEq("InFirstAlbumOnly.jpg", entries[0].name);

    verifyAllJPEGs(directoryEntry, ["InFirstAlbumOnly.jpg"],
                   chrome.test.succeed);
  }

  getAndVerifyDirectoryEntry(picasaGallery.root,
                             "folders/folder2 1899-12-30", verify);
}

function getTest(testFunction) {
  function getMediaFileSystemsList() {
    mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
  }

  function getMediaFileSystemsCallback(results) {
    for (var i = 0; i < results.length; ++i) {
      var properties = mediaGalleries.getMediaFileSystemMetadata(results[i]);
      if (properties.name == "Picasa") {
        testFunction(results[i]);
        return;
      }
    }
    chrome.test.fail("Picasa gallery not found");
  }

  return function() {
    getMediaFileSystemsList();
  }
}

chrome.test.runTests([
  getTest(GalleryPropertiesTest),
  getTest(RootListingTest),
  getTest(AlbumsListingTest),
  getTest(FoldersListingTest),
  getTest(Album1ListingTest),
  getTest(Album2ListingTest),
  getTest(Folder1ListingTest),
  getTest(Folder2ListingTest),
]);

// Create a dummy window to prevent the ExtensionProcessManager from suspending
// the chrome-test app. Needed because the FileReader events do not qualify as
// pending callbacks, so the app looks dormant.
chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('dummy.html', {
    bounds: {
      width: 800,
      height: 600,
      left: 100,
      top: 100
    },
    minWidth: 800,
    minHeight: 600
  });
});
