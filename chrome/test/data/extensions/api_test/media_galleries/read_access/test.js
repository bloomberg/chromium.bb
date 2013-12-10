// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;
var expectedGalleryEntryLength;

function TestFirstFilesystem(verifyFilesystem) {
  function getMediaFileSystemsList() {
    mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
  }

  function getMediaFileSystemsCallback(results) {
    chrome.test.assertEq(1, results.length);
    verifyFilesystem(results[0]);
  }

  getMediaFileSystemsList();
}

function ReadDirectoryTest() {
  function verifyFilesystem(filesystem) {
    verifyDirectoryEntry(filesystem.root, verify);
  }

  function verify(directoryEntry, entries) {
    chrome.test.assertEq(1, entries.length);
    chrome.test.assertFalse(entries[0].isDirectory);
    chrome.test.assertEq("test.jpg", entries[0].name);
    chrome.test.succeed();
  }

  TestFirstFilesystem(verifyFilesystem);
}

function ReadFileToBytesTest() {
  function verifyFilesystem(filesystem) {
    verifyJPEG(filesystem.root, "test.jpg", expectedGalleryEntryLength,
               chrome.test.succeed);
  }

  TestFirstFilesystem(verifyFilesystem);
}

function GetMetadataTest() {
  function verifyFilesystem(filesystem) {
    filesystem.root.getFile("test.jpg", {create: false}, verifyFileEntry,
      chrome.test.fail);
  }

  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail)
  }

  function verifyFile(file) {
    mediaGalleries.getMetadata(file, {}, verifyMetadata);
  }

  function verifyMetadata(metadata) {
    chrome.test.assertEq("image/jpeg", metadata.mimeType);
    chrome.test.succeed();
  }

  TestFirstFilesystem(verifyFilesystem);
}

function GetMediaFileSystemMetadata() {
  function verifyFilesystem(filesystem) {
    var metadata = mediaGalleries.getMediaFileSystemMetadata(filesystem);
    checkMetadata(metadata);
    chrome.test.succeed();
  }

  TestFirstFilesystem(verifyFilesystem);
}

function GetAllMediaFileSystemMetadata() {
  function verifyMetadataList(metadataList) {
    chrome.test.assertEq(1, metadataList.length)
    checkMetadata(metadataList[0]);
    chrome.test.succeed();
  }

  mediaGalleries.getAllMediaFileSystemMetadata(verifyMetadataList);
}

CreateDummyWindowToPreventSleep();

chrome.test.getConfig(function(config) {
  customArg = JSON.parse(config.customArg);
  expectedGalleryEntryLength = customArg[0];

  chrome.test.runTests([
    ReadDirectoryTest,
    ReadFileToBytesTest,
    GetMetadataTest,
    GetMediaFileSystemMetadata,
    GetAllMediaFileSystemMetadata,
  ]);
})
