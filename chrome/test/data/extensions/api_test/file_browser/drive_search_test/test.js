// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Extension apitests for drive search methods.
 * There are three tests functions run:
 * - loadFileSystem() which requests local file system and verifies the drive
 *   mount point exists.
 * - driveSearch() which tests chrome.fileBrowserPrivate.searchDrive function.
 * - driveMetadataSearch() which tests
 *   chrome.fileBrowserPrivate.searchDriveMetadata function.
 *
 * For both search test functions, the test verifies that the file system
 * operations can be performed on the returned result entries. For file entries
 * by trying FileEntry.file function and for directory entries by trying
 * to run the deirectories directory reader.
 */

/**
 * Verifies that a file snapshot can be created for the entry.
 * On failure, the current test is ended with failure.
 * On success, |successCallback| is called.
 *
 * @param {FileEntry} entry File entry to be tested.
 * @param {function()} successCallback The function called when the file entry
 *     is successfully verified.
 */
function verifyFile(entry, successCallback) {
  chrome.test.assertFalse(!entry.file, 'Entry has no file method.');
  entry.file(successCallback,
             chrome.test.fail.bind(null, 'Error reading result file.'));
}

/**
 * Verifies that a directory reader can be created and used for the entry.
 * On failure, the current test is ended with failure.
 * On success, |successCallback| is called.
 *
 * @param {DirectoryEntry} entry Directory entry to be tested.
 * @param {function()} successCallback The function called when the dir entry
 *     is successfully verified.
 */
function verifyDirectory(entry, successCallback) {
  chrome.test.assertFalse(!entry.createReader,
                          'Entry has no createReader method.');
  var reader = entry.createReader();

  reader.readEntries(successCallback,
                     chrome.test.fail.bind(null, 'Error reading directory.'));
}

/**
 * Returns the fuction that should be used to verify the entry whose type is
 * specified by |type|.
 *
 * @param {string} type The entry type. Can be either 'file' or 'dir'.
 * @return {function(Entry, successCallback)}
 */
function getEntryVerifier(type) {
  if (type == 'file')
    return verifyFile;

  if (type == 'dir')
    return verifyDirectory;

  return null;
}

chrome.test.runTests([
  // Loads filesystem that contains drive mount point.
  function loadFileSystem() {
    chrome.fileBrowserPrivate.requestLocalFileSystem(
      function (fileSystem) {
        chrome.test.assertFalse(!fileSystem, 'Failed to get file system.');
        fileSystem.root.getDirectory('drive/test_dir', {create: false},
            // Also read a non-root directory. This will initiate loading of
            // the full resource metadata. As of now, 'search' only works
            // with the resource metadata fully loaded. crbug.com/181075
            function(entry) {
              var reader = entry.createReader();
              reader.readEntries(
                  chrome.test.succeed,
                  chrome.test.fail.bind(null, 'Error reading directory.'));
            },
            chrome.test.fail.bind(null, 'Unable to get drive mount point.'));
      });
  },

  // Tests chrome.fileBrowserPrivate.searchDrive method.
  function driveSearch() {
    var testCases = [
      {
        nextFeed: '',
        expectedPath: '/drive/test_dir/empty_test_dir',
        expectedType: 'dir',
        expectedNextFeed: 'http://localhost/?start-offset=1&max-results=1',
      },
      {
        // The same as the previous test case's expected next feed.
        nextFeed: 'http://localhost/?start-offset=1&max-results=1',
        expectedPath: '/drive/test_dir/empty_test_file.foo',
        expectedType: 'file',
        expectedNextFeed: '',
      }
    ];

    function runNextQuery() {
      // If there is no more queries the test ended successfully.
      if (testCases.length == 0) {
        chrome.test.succeed();
        return;
      }

      var testCase = testCases.shift();

      // Each search query should return exactly one result (this should be
      // ensured by Chrome part of the test).
      chrome.fileBrowserPrivate.searchDrive(
          {query: 'empty', sharedWithMe: false, nextFeed: testCase.nextFeed},
          function(entries, nextFeed) {
            chrome.test.assertFalse(!entries);
            chrome.test.assertEq(1, entries.length);
            chrome.test.assertEq(testCase.expectedPath,
                                 entries[0].entry.fullPath);
            chrome.test.assertEq(testCase.expectedNextFeed, nextFeed);

            var verifyEntry = getEntryVerifier(testCase.expectedType);
            chrome.test.assertFalse(!verifyEntry);

            // The callback will be called only it the entry is successfully
            // verified, otherwise the test function will fail.
            verifyEntry(entries[0].entry, runNextQuery);
      });
    }

    runNextQuery();
  },

  // Tests chrome.fileBrowserPrivate.searchDriveMetadata method.
  function driveMetadataSearch() {
    // The results should be sorted by (lastAccessed, lastModified) pair. The
    // sort should be decending. The comments above each expected result
    // represent their (lastAccessed, lastModified) pair. These values are set
    // in remote_file_system_api_test_root_feed.json.
    // The API should return 5 results, even though there are more than five
    // matches in the test file system.
    var expectedResults = [
        // (2012-01-02T00:00:01.000Z, 2012-01-02T00:00:0.000Z)
        {path: '/drive/test_dir', type: 'dir'},
        // (2012-01-02T00:00:00.000Z, 2012-01-01T00:00:00.005Z)
        {path: '/drive/test_dir/test_file.xul', type: 'file'},
        // (2012-01-02T00:00:00.000Z, 2011-04-03T11:11:10.000Z)
        {path: '/drive/test_dir/test_file.tiff', type: 'file'},
        // (2012-01-01T11:00:00.000Z, 2012-01-01T10:00:30.00Z)
        {path: '/drive/test_dir/test_file.xul.foo', type: 'file'},
    ];

    chrome.fileBrowserPrivate.searchDriveMetadata(
        'test',
        function(entries) {
          chrome.test.assertFalse(!entries);
          chrome.test.assertEq(expectedResults.length, entries.length);

          function verifySearchResultAt(resultIndex) {
            if (resultIndex == expectedResults.length) {
              chrome.test.succeed();
              return;
            }

            chrome.test.assertFalse(!entries[resultIndex]);
            chrome.test.assertFalse(!entries[resultIndex].entry);
            chrome.test.assertEq(expectedResults[resultIndex].path,
                                 entries[resultIndex].entry.fullPath);

            var verifyEntry =
                getEntryVerifier(expectedResults[resultIndex].type);
            chrome.test.assertFalse(!verifyEntry);

            // The callback will be called only if the entry is successfully
            // verified, otherwise the test function will fail.
            verifyEntry(entries[resultIndex].entry,
                        verifySearchResultAt.bind(null, resultIndex + 1));
          }

          verifySearchResultAt(0);
        });
  }
]);
