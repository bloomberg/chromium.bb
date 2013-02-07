// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifySearchResult(entries,
                            nextFeed,
                            expectedResult,
                            expectedNextFeed) {
  chrome.test.assertTrue(typeof(entries) != "undefined");
  chrome.test.assertEq(1, entries.length);
  chrome.test.assertEq(expectedResult, entries[0].entry.fullPath);
  chrome.test.assertEq(expectedNextFeed, nextFeed);
}

chrome.test.runTests([
  function loadFileSystem() {
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      function (fs) {
        chrome.test.assertTrue(typeof(fs) != "undefined");
        fs.root.getDirectory('drive', {create: false}, chrome.test.succeed,
            errorCallback.bind(null, 'Unable to get drive root '));
      });
  },
  function driveSearch() {
    var params = {
        'query': 'F',  // Matches "Folder" and "File.aBc".
        'sharedWithMe': false,
        'nextFeed': ''
    };

    chrome.fileBrowserPrivate.searchDrive(
        params,
        function(entries, nextFeed) {
          // The first search should return the followings:
          var expectedResult = '/drive/Folder';
          var expectedNextFeed =
            'http://localhost/?start-offset=1&max-results=1';
          verifySearchResult(
            entries, nextFeed, expectedResult, expectedNextFeed);
          var directoryEntry = entries[0].entry;

          var nextParams = {
              'query': 'F',   // Matches "Folder" and "File.aBc".
              'sharedWithMe': false,
              'nextFeed': nextFeed
          };
          chrome.fileBrowserPrivate.searchDrive(
              nextParams,
              function(entries, nextFeed) {
                // The second search should return the followings:
                var expectedResult = '/drive/Folder/File.aBc';
                var expectedNextFeed = '';
                verifySearchResult(entries,
                                   nextFeed,
                                   expectedResult,
                                   expectedNextFeed);
                var fileEntry = entries[0].entry;

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
        });
  }
]);
