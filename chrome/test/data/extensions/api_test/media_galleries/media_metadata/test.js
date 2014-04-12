// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

function RunMetadataTest(filename, callOptions, verifyMetadataFunction) {
  function getMediaFileSystemsCallback(results) {
    chrome.test.assertEq(1, results.length);
    var gallery = results[0];
    gallery.root.getFile(filename, {create: false}, verifyFileEntry,
      chrome.test.fail);
  }

  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail)
  }

  function verifyFile(file) {
    mediaGalleries.getMetadata(file, callOptions, verifyMetadataFunction);
  }

  mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
}

function ImageMIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("image/jpeg", metadata.mimeType);
    chrome.test.succeed();
  }

  RunMetadataTest("test.jpg", {metadataType: 'mimeTypeOnly'}, verifyMetadata);
}

function ImageTagsTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("image/jpeg", metadata.mimeType);
    chrome.test.assertEq(5616, metadata.width);
    chrome.test.assertEq(3744, metadata.height);
    chrome.test.assertEq(0, metadata.rotation);
    chrome.test.assertEq(300.0, metadata.xResolution);
    chrome.test.assertEq(300.0, metadata.yResolution);
    chrome.test.assertEq("Canon", metadata.cameraMake);
    chrome.test.assertEq("Canon EOS 5D Mark II", metadata.cameraModel);
    chrome.test.assertEq(0.01, metadata.exposureTimeSeconds);
    chrome.test.assertFalse(metadata.flashFired);
    chrome.test.assertEq(3.2, metadata.fNumber);
    chrome.test.assertEq(100, metadata.focalLengthMm);
    chrome.test.assertEq(1600, metadata.isoEquivalent);
    chrome.test.succeed();
  }

  RunMetadataTest("test.jpg", {}, verifyMetadata);
}

function MP3MIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq(undefined, metadata.title);
    chrome.test.succeed();
  }

  RunMetadataTest("id3_png_test.mp3", {metadataType: 'mimeTypeOnly'},
                  verifyMetadata);
}

function MP3TagsTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq("Airbag", metadata.title);
    chrome.test.assertEq("Radiohead", metadata.artist);
    chrome.test.assertEq("OK Computer", metadata.album);
    chrome.test.assertEq(1, metadata.track);
    chrome.test.assertEq("Alternative", metadata.genre);

    chrome.test.assertEq("OK Computer", metadata.rawTags["album"]);
    chrome.test.assertEq("Radiohead", metadata.rawTags["artist"]);
    chrome.test.assertEq("1997", metadata.rawTags["date"]);
    chrome.test.assertEq("Lavf54.4.100", metadata.rawTags["encoder"]);
    chrome.test.assertEq("Alternative", metadata.rawTags["genre"]);
    chrome.test.assertEq("Airbag", metadata.rawTags["title"]);
    chrome.test.assertEq("1", metadata.rawTags["track"]);

    chrome.test.succeed();
  }

  return RunMetadataTest("id3_png_test.mp3", {}, verifyMetadata);
}

function RotatedVideoTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("video/mp4", metadata.mimeType);
    chrome.test.assertEq(90, metadata.rotation);

    chrome.test.assertEq("isom3gp4", metadata.rawTags["compatible_brands"]);
    chrome.test.assertEq("2014-02-11 00:39:25",
                         metadata.rawTags["creation_time"]);
    chrome.test.assertEq("VideoHandle", metadata.rawTags["handler_name"]);
    chrome.test.assertEq("eng", metadata.rawTags["language"]);
    chrome.test.assertEq("isom", metadata.rawTags["major_brand"]);
    chrome.test.assertEq("0", metadata.rawTags["minor_version"]);
    chrome.test.assertEq("90", metadata.rawTags["rotate"]);

    chrome.test.succeed();
  }

  return RunMetadataTest("90rotation.mp4", {}, verifyMetadata);
}

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  var useProprietaryCodecs = customArg[0];

  // Should still be able to sniff MP3 MIME type without proprietary codecs.
  var testsToRun = [
    ImageMIMETypeOnlyTest,
    ImageTagsTest
  ];

  if (useProprietaryCodecs) {
    testsToRun = testsToRun.concat([
      MP3MIMETypeOnlyTest,
      MP3TagsTest,
      RotatedVideoTest
    ]);
  }

  chrome.test.runTests(testsToRun);
});
