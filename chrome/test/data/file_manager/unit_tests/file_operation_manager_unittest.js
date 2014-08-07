// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Reports the result of promise to the test system.
 * @param {Promise} promise Promise to be fulfilled or rejected.
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function reportPromise(promise, callback) {
  promise.then(
      callback.bind(null, false),
      function(error) {
        if (error instanceof FileOperationManager.Error) {
          console.log('FileOperationManager.Error: code=' + error.code);
        } else {
          console.log(error.stack || error.name || error);
        }
        callback(true);
      });
}

/**
 * Tests the fileOperationUtil.resolvePath function.
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testResolvePath(callback) {
  var fileEntry = new MockFileEntry('testVolume', '/file', {});
  var directoryEntry = new MockDirectoryEntry('testVolume', '/directory', {});
  var root = new MockDirectoryEntry('testVolume', '/', {
    '/file': fileEntry,
    '/directory': directoryEntry
  });
  var rootPromise = fileOperationUtil.resolvePath(root, '/');
  var filePromise = fileOperationUtil.resolvePath(root, '/file');
  var directoryPromise = fileOperationUtil.resolvePath(root, '/directory');
  var errorPromise = fileOperationUtil.resolvePath(root, '/not_found').then(
      function() {
        assertTrue(false, 'The NOT_FOUND error is not reported.');
      },
      function(error) {
        assertEquals('NotFoundError', error.name);
      });
  reportPromise(Promise.all([
    rootPromise,
    filePromise,
    directoryPromise,
    errorPromise
  ]).then(function(results) {
    assertArrayEquals([
      root,
      fileEntry,
      directoryEntry,
      undefined
    ], results);
  }), callback);
}

/**
 * Tests the fileOperationUtil.deduplicatePath
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testDeduplicatePath(callback) {
  var directoryEntry1 = new MockDirectoryEntry('testVolume', '/directory', {});
  var directoryEntry2 = new MockDirectoryEntry(
      'testVolume',
      '/directory',
      {'file.txt': new MockFileEntry('testVolume', '/file.txt', {})});
  var directoryEntry3 = new MockDirectoryEntry(
      'testVolume',
      '/directory',
      {
        'file.txt': new MockFileEntry('testVolume', '/file.txt', {}),
        'file (1).txt': new MockFileEntry('testVolume', '/file (1).txt', {}),
        'file (2).txt': new MockFileEntry('testVolume', '/file (2).txt', {}),
        'file (3).txt': new MockFileEntry('testVolume', '/file (3).txt', {}),
        'file (4).txt': new MockFileEntry('testVolume', '/file (4).txt', {}),
        'file (5).txt': new MockFileEntry('testVolume', '/file (5).txt', {}),
        'file (6).txt': new MockFileEntry('testVolume', '/file (6).txt', {}),
        'file (7).txt': new MockFileEntry('testVolume', '/file (7).txt', {}),
        'file (8).txt': new MockFileEntry('testVolume', '/file (8).txt', {}),
        'file (9).txt': new MockFileEntry('testVolume', '/file (9).txt', {})
      });

  var nonExistingPromise =
      fileOperationUtil.deduplicatePath(directoryEntry1, 'file.txt').
      then(function(path) {
        assertEquals('file.txt', path);
      });
  var existingPathPromise =
      fileOperationUtil.deduplicatePath(directoryEntry2, 'file.txt').
      then(function(path) {
        assertEquals('file (1).txt', path);
      });
  var failedPromise =
      fileOperationUtil.deduplicatePath(directoryEntry3, 'file.txt').
      then(function() {
        assertTrue(false, 'FileOperationManager.Error is not reported.');
      }, function(error) {
        assertTrue(error instanceof FileOperationManager.Error);
        assertEquals(util.FileOperationErrorType.TARGET_EXISTS, error.code);
      });

  var testPromise = Promise.all([
    nonExistingPromise,
    existingPathPromise,
    failedPromise
  ]);
  reportPromise(testPromise, callback);
}
