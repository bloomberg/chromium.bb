// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Mock of chrome.runtime.
 * @type {Object}
 * @const
 */
chrome.runtime = {
  lastError: null
};

/**
 * Mock of chrome.power.
 * @type {Object}
 * @const
 */
chrome.power = {
  requestKeepAwake: function() {
    chrome.power.keepAwakeRequested = true;
  },
  releaseKeepAwake: function() {
    chrome.power.keepAwakeRequested = false;
  },
  keepAwakeRequested: false
};

/**
 * Mock of chrome.fileBrowserPrivate.
 * @type {Object}
 * @const
 */
chrome.fileBrowserPrivate = {
  onCopyProgress: {
    addListener: function(callback) {
      chrome.fileBrowserPrivate.onCopyProgress.listener_ = callback;
    },
    removeListener: function() {
      chrome.fileBrowserPrivate.onCopyProgress.listener_ = null;
    },
    listener_: null
  },
  startCopy: function(source, destination, newName, callback) {
    var id = 1;
    var events = [
      'begin_copy_entry',
      'progress',
      'end_copy_entry',
      'success'
    ].map(function(type) {
      return [id, {type: type, sourceUrl: source, destinationUrl: destination}];
    });
    var sendEvent = function(index) {
      // Call the function asynchronously.
      return Promise.resolve().then(function() {
        chrome.fileBrowserPrivate.onCopyProgress.listener_.apply(
            null, events[index]);
        if (index + 1 < events.length)
          return sendEvent(index + 1);
        else
          return null;
      }.bind(this));
    }.bind(this);
    callback(id);
    sendEvent(0).catch(function(error) {
      console.log(error.stack || error);
      window.onerror();
    });
  }
};

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
 * Test target.
 * @type {FileOperationManager}
 */
var fileOperationManager;

/**
 * Initializes the test environment.
 */
function setUp() {
  fileOperationManager = new FileOperationManager();
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

/**
 * Tests the fileOperationUtil.paste.
 */
function testCopy(callback) {
  // Prepare entries and their resolver.
  var sourceEntries =
      [new MockFileEntry('testVolume', '/test.txt', {size: 10})];
  var targetEntry = new MockDirectoryEntry('testVolume', '/', {});
  window.webkitResolveLocalFileSystemURL = function(url, success, failure) {
    if (url === sourceEntries[0].toURL())
      success(sourceEntries[0]);
    else if (url === targetEntry.toURL())
      success(targetEntry);
    else
      failure();
  };

  // Observing manager's events.
  var eventsPromise = new Promise(function(fulfill) {
    var events = [];
    fileOperationManager.addEventListener('copy-progress', function(event) {
      events.push(event);
      if (event.reason === 'SUCCESS')
        fulfill(events);
    });
    fileOperationManager.addEventListener('entry-changed', function(event) {
      events.push(event);
    });
  });

  // Verify the events.
  reportPromise(eventsPromise.then(function(events) {
    var firstEvent = events[0];
    assertEquals('BEGIN', firstEvent.reason);
    assertEquals(1, firstEvent.status.numRemainingItems);
    assertEquals(0, firstEvent.status.processedBytes);
    assertEquals(1, firstEvent.status.totalBytes);

    var lastEvent = events[events.length - 1];
    assertEquals('SUCCESS', lastEvent.reason);
    assertEquals(0, lastEvent.status.numRemainingItems);
    assertEquals(10, lastEvent.status.processedBytes);
    assertEquals(10, lastEvent.status.totalBytes);

    assertTrue(events.some(function(event) {
      return event.type === 'entry-changed' &&
          event.entry === targetEntry;
    }));
  }), callback);

  fileOperationManager.paste(sourceEntries, targetEntry, false);
}
