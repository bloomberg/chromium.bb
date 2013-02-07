// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function loadFileSystem() {
    chrome.fileBrowserPrivate.requestLocalFileSystem(
      function (fs) {
        chrome.test.assertTrue(typeof(fs) != "undefined");
        fs.root.getDirectory('drive', {create: false}, chrome.test.succeed,
            errorCallback.bind(null, 'Unable to get drive root '));
      });
  },

  function driveMetadataSearch() {
    chrome.fileBrowserPrivate.searchDriveMetadata(
        'F',  // Matches "File.aBc" and "Folder".
        function(entries) {
          chrome.test.assertTrue(typeof(entries) != "undefined");
          chrome.test.assertEq(2, entries.length);
          // "File.aBc" comes first as it's newer.
          var fileEntry = entries[0].entry;
          var directoryEntry = entries[1].entry;
          chrome.test.assertEq('/drive/Folder/File.aBc', fileEntry.fullPath);
          chrome.test.assertEq('/drive/Folder', directoryEntry.fullPath);

          // Check if directoryEntry ('/drive/Folder') is accessible
          // and contains one child.
          var expectedNumChildren = 1;
          var directoryVerifier =
              verifyDirectoryAccessible.bind(null,
                                             directoryEntry,
                                             expectedNumChildren,
                                             chrome.test.succeed,
                                             errorCallback);

          // Check if the fileEntry ('/drive/Folder/File.aBc' is
          // accessible). Note that |directoryVerifier| will be called
          // upon success of verifyFileAccessible.
          verifyFileAccessible(fileEntry, directoryVerifier,
                               errorCallback);
        });
  }
]);
