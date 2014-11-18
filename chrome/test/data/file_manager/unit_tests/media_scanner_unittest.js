// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dummy private APIs.
 */
var chrome;

/**
 * Callbacks registered by setTimeout.
 * @type {Array.<function>}
 */
var timeoutCallbacks;


// Set up the test components.
function setUp() {
}

/**
 * Creates a subdirectory within a temporary file system for testing.
 * @param {string} directoryName Name of the test directory to create.  Must be
 *     unique within this test suite.
 */
function makeTestFilesystemRoot(directoryName) {
  function makeTestFilesystem() {
    return new Promise(function(resolve, reject) {
      window.webkitRequestFileSystem(
          window.TEMPORARY,
          1024*1024,
          resolve,
          reject);
    });
  }

  return makeTestFilesystem()
      .then(
          // Create a directory, pretend that's the root.
          function(fs) {
            return new Promise(function(resolve, reject) {
              fs.root.getDirectory(
                    directoryName,
                    {
                      create: true,
                      exclusive: true
                    },
                  resolve,
                  reject);
            });
          });
}

/**
 * Creates a set of files in the given directory.
 * @param {!Array<!Array|string>} filenames A (potentially nested) array of
 *     strings, reflecting a directory structure.
 * @param <!DirectoryEntry> The root of the directory tree.
 */
function populateDir(filenames, dir) {
  return Promise.all(
      filenames.map(function(filename) {
        if (filename instanceof Array) {
          return new Promise(function(resolve, reject) {
            dir.getDirectory(filename[0], {create: true}, resolve, reject);
          }).then(populateDir.bind(null, filename));
        } else {
          return new Promise(function(resolve, reject) {
            dir.getFile(filename, {create: true}, resolve, reject);
          });
        }
      }));
}

/**
 * Verifies that scanning an empty filesystem produces an empty list.
 */
function testEmptyList(errorIf) {
  var scanner = new MediaScanner([]);
  scanner.getFiles().then(function(files) {
    errorIf(files.length !== 0);
  });
}

/**
 * Verifies that scanning a simple single-level directory structure works.
 */
function testSingleLevel(errorIf) {
  var filenames = [
      'foo',
      'foo.jpg',
      'bar.gif',
      'baz.avi',
      'foo.mp3',
      'bar.txt'
  ];
  var expectedFiles = [
      '/testSingleLevel/foo.jpg',
      '/testSingleLevel/bar.gif',
      '/testSingleLevel/baz.avi'
  ];
  makeTestFilesystemRoot('testSingleLevel')
      .then(
          /**
           * Creates the test directory and populates it.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            populateDir(filenames, root);
            return root;
          })
      .then(
          /**
           * Scans the directory.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            var scanner = new MediaScanner([root]);
            return scanner.getFiles();
          })
      .then(
          /**
           * Verifies the results of the media scan.
           * @param {!Array.<!FileEntry>} scanResults
           */
          function(scanResults) {
            assertEquals(expectedFiles.length, scanResults.length);
            scanResults.forEach(function(result) {
              // Verify that the scanner only returns files.
              assertTrue(result.isFile, result.fullPath + ' is not a file');
              assertTrue(expectedFiles.indexOf(result.fullPath) != -1,
                  result.fullPath + ' not found in control set');
            });
            // Signal test completion with no errors.
            errorIf(false);
          })
      .catch(
          function(e) {
            // Catch failures and print them.
            console.error(e);
            errorIf(e);
          });
}

function testMultiLevel(errorIf) {
  var filenames = [
      'foo.jpg',
      'bar',
      [
          'foo.0',
          'bar.0.jpg'
      ],
      [
          'foo.1',
          'bar.1.gif',
          [
              'foo.1.0',
              'bar.1.0.avi'
          ]
      ]
  ];
  var expectedFiles = [
      '/testMultiLevel/foo.jpg',
      '/testMultiLevel/foo.0/bar.0.jpg',
      '/testMultiLevel/foo.1/bar.1.gif',
      '/testMultiLevel/foo.1/foo.1.0/bar.1.0.avi'
  ];

  makeTestFilesystemRoot('testMultiLevel')
      .then(
          /**
           * Creates the test directory and populates it.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            populateDir(filenames, root);
            return root;
          })
      .then(
          /**
           * Scans the directory.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            var scanner = new MediaScanner([root]);
            return scanner.getFiles();
          })
      .then(
          /**
           * Verifies the results of the media scan.
           * @param {!Array.<!FileEntry>} scanResults
           */
          function(scanResults) {
            assertEquals(expectedFiles.length, scanResults.length);
            scanResults.forEach(function(result) {
              // Verify that the scanner only returns files.
              assertTrue(result.isFile, result.fullPath + ' is not a file');
              assertTrue(expectedFiles.indexOf(result.fullPath) != -1,
                  result.fullPath + ' not found in control set');
            });
            // Signal test completion with no errors.
            errorIf(false);
          })
      .catch(
          function(e) {
            // Catch failures and print them.
            console.error(e);
            errorIf(e);
          });

  errorIf(false);
}

function testMultipleDirectories(errorIf) {
  var filenames = [
      'foo',
      'bar',
      [
          'foo.0',
          'bar.0.jpg'
      ],
      [
          'foo.1',
          'bar.1.jpg',
      ]
  ];
  // Expected file paths from the scan.  We're scanning the two subdirectories
  // only.
  var expectedFiles = [
      '/testMultipleDirectories/foo.0/bar.0.jpg',
      '/testMultipleDirectories/foo.1/bar.1.jpg'
  ];

  var getDirectory = function(root, dirname) {
    return new Promise(function(resolve, reject) {
      root.getDirectory(
          dirname, {create: false}, resolve, reject);
    });
  };
  makeTestFilesystemRoot('testMultipleDirectories')
      .then(
          /**
           * Creates the test directory and populates it.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            populateDir(filenames, root);
            return root;
          })
      .then(
          /**
           * Scans the directories.
           * @param {!DirectoryEntry} root
           */
          function(root) {
            return Promise.all(['foo.0', 'foo.1'].map(
                getDirectory.bind(null, root))).then(
                    function(directories) {
                      var scanner = new MediaScanner(directories);
                      return scanner.getFiles();
                    });
          })
      .then(
          /**
           * Verifies the results of the media scan.
           * @param {!Array.<!FileEntry>} scanResults
           */
          function(scanResults) {
            assertEquals(expectedFiles.length, scanResults.length);
            scanResults.forEach(function(result) {
              // Verify that the scanner only returns files.
              assertTrue(result.isFile, result.fullPath + ' is not a file');
              assertTrue(expectedFiles.indexOf(result.fullPath) != -1,
                  result.fullPath + ' not found in control set');
            });
            // Signal test completion with no errors.
            errorIf(false);
          })
      .catch(
          function(e) {
            // Catch failures and print them.
            console.error(e);
            errorIf(e);
          });
}
