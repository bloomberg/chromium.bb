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
          console.error('FileOperationManager.Error: code=' + error.code);
        } else {
          console.error(error.stack || error.name || error);
        }
        callback(true);
      });
}

/**
 * Size of directory.
 * @type {number}
 * @const
 */
var DIRECTORY_SIZE = -1;

/**
 * Creates test file system.
 * @param {string} id File system ID.
 * @param {Object.<string, number>} entries Map of entries' paths and their
 *     size. If the size is equals to DIRECTORY_SIZE, the entry is derectory.
 */
function createTestFileSystem(id, entries) {
  var fileSystem = new TestFileSystem(id);
  for (var path in entries) {
    if (entries[path] === DIRECTORY_SIZE) {
      fileSystem.entries[path] = new MockDirectoryEntry(fileSystem, path);
    } else {
      fileSystem.entries[path] =
          new MockFileEntry(fileSystem, path, {size: entries[path]});
    }
  }
  return fileSystem;
}

/**
 * Resolves URL on the file system.
 * @param {FakeFileSystem} fileSystem Fake file system.
 * @param {string} url URL.
 * @param {function(MockEntry)} success Success callback.
 * @param {function()} failure Failure callback.
 */
function resolveTestFileSystemURL(fileSystem, url, success, failure) {
  for (var name in fileSystem.entries) {
    var entry = fileSystem.entries[name];
    if (entry.toURL() == url) {
      success(entry);
      return;
    }
  }
  failure();
}

/**
 * Waits for events until 'success'.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @return {Promise} Promise to be fulfilled with an event list.
 */
function waitForEvents(fileOperationManager) {
  return new Promise(function(fulfill) {
    var events = [];
    fileOperationManager.addEventListener('copy-progress', function(event) {
      events.push(event);
      if (event.reason === 'SUCCESS')
        fulfill(events);
    });
    fileOperationManager.addEventListener('entries-changed', function(event) {
      events.push(event);
    });
    fileOperationManager.addEventListener('delete', function(event) {
      events.push(event);
      if (event.reason === 'SUCCESS')
        fulfill(events);
    });
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
  var fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file': 10,
    '/directory': DIRECTORY_SIZE
  });
  var root = fileSystem.root;
  var rootPromise = fileOperationUtil.resolvePath(root, '/');
  var filePromise = fileOperationUtil.resolvePath(root, '/file');
  var directoryPromise = fileOperationUtil.resolvePath(root, '/directory');
  var errorPromise = fileOperationUtil.resolvePath(root, '/not_found').then(
      function() { assertTrue(false, 'The NOT_FOUND error is not reported.'); },
      function(error) { return error.name; });
  reportPromise(Promise.all([
    rootPromise,
    filePromise,
    directoryPromise,
    errorPromise
  ]).then(function(results) {
    assertArrayEquals([
      fileSystem.entries['/'],
      fileSystem.entries['/file'],
      fileSystem.entries['/directory'],
      'NotFoundError'
    ], results);
  }), callback);
}

/**
 * Tests the fileOperationUtil.deduplicatePath
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testDeduplicatePath(callback) {
  var fileSystem1 = createTestFileSystem('testVolume', {'/': DIRECTORY_SIZE});
  var fileSystem2 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10
  });
  var fileSystem3 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/file (6).txt': 10,
    '/file (7).txt': 10,
    '/file (8).txt': 10,
    '/file (9).txt': 10,
  });

  var nonExistingPromise =
      fileOperationUtil.deduplicatePath(fileSystem1.root, 'file.txt').
      then(function(path) {
        assertEquals('file.txt', path);
      });
  var existingPathPromise =
      fileOperationUtil.deduplicatePath(fileSystem2.root, 'file.txt').
      then(function(path) {
        assertEquals('file (1).txt', path);
      });
  var failedPromise =
      fileOperationUtil.deduplicatePath(fileSystem3.root, 'file.txt').
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
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testCopy(callback) {
  // Prepare entries and their resolver.
  var fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL =
      resolveTestFileSystemURL.bind(null, fileSystem);

  chrome.fileBrowserPrivate.startCopy =
      function(source, destination, newName, callback) {
        var makeStatus = function(type) {
          return {type: type, sourceUrl: source, destinationUrl: destination};
        };
        callback(1);
        var listener = chrome.fileBrowserPrivate.onCopyProgress.listener_;
        listener(1, makeStatus('begin_copy_entry'));
        listener(1, makeStatus('progress'));
        var newPath = joinPath('/', newName);
        fileSystem.entries[newPath] =
            fileSystem.entries['/test.txt'].clone(newPath);
        listener(1, makeStatus('end_copy_entry'));
        listener(1, makeStatus('success'));
      };

  // Observing manager's events.
  var eventsPromise = waitForEvents(fileOperationManager);

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
      return event.type === 'entries-changed' &&
          event.kind === util.EntryChangedKind.CREATED &&
          event.entries[0].fullPath === '/test (1).txt';
    }));

    assertFalse(events.some(function(event) {
      return event.type === 'delete';
    }));
  }), callback);

  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      fileSystem.entries['/'],
      false);
}

/**
 * Tests the fileOperationUtil.paste for move.
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testMove(callback) {
  // Prepare entries and their resolver.
  var fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/directory': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL =
      resolveTestFileSystemURL.bind(null, fileSystem);

  // Observing manager's events.
  var eventsPromise = waitForEvents(fileOperationManager);

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
    assertEquals(1, lastEvent.status.processedBytes);
    assertEquals(1, lastEvent.status.totalBytes);

    assertTrue(events.some(function(event) {
      return event.type === 'entries-changed' &&
          event.kind === util.EntryChangedKind.DELETED &&
          event.entries[0].fullPath === '/test.txt';
    }));

    assertTrue(events.some(function(event) {
      return event.type === 'entries-changed' &&
          event.kind === util.EntryChangedKind.CREATED &&
          event.entries[0].fullPath === '/directory/test.txt';
    }));

    assertFalse(events.some(function(event) {
      return event.type === 'delete';
    }));
  }), callback);

  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      fileSystem.entries['/directory'],
      true);
}

/**
 * Tests the fileOperationUtil.deleteEntries.
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testDelete(callback) {
  // Prepare entries and their resolver.
  var fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL =
      resolveTestFileSystemURL.bind(null, fileSystem);

  // Observing manager's events.
  reportPromise(waitForEvents(fileOperationManager).then(function(events) {
    assertEquals('delete', events[0].type);
    assertEquals('BEGIN', events[0].reason);
    assertEquals(10, events[0].totalBytes);
    assertEquals(0, events[0].processedBytes);

    var lastEvent = events[events.length - 1];
    assertEquals('delete', lastEvent.type);
    assertEquals('SUCCESS', lastEvent.reason);
    assertEquals(10, lastEvent.totalBytes);
    assertEquals(10, lastEvent.processedBytes);

    assertFalse(events.some(function(event) {
      return event.type === 'copy-progress';
    }));
  }), callback);

  fileOperationManager.deleteEntries([fileSystem.entries['/test.txt']]);
}

/**
 * Tests the fileOperationUtil.zipSelection.
 * @param {function(boolean:hasError)} callback Callback to be passed true on
 *     error.
 */
function testZip(callback) {
  // Prepare entries and their resolver.
  var fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL =
      resolveTestFileSystemURL.bind(null, fileSystem);
  chrome.fileBrowserPrivate.zipSelection =
      function(parentURL, sources, newName, success, error) {
        var newPath = joinPath('/', newName);
        var newEntry = new MockFileEntry(fileSystem, newPath, {size: 10});
        fileSystem.entries[newPath] = newEntry;
        success(newEntry);
      };

  // Observing manager's events.
  reportPromise(waitForEvents(fileOperationManager).then(function(events) {
    assertEquals('copy-progress', events[0].type);
    assertEquals('BEGIN', events[0].reason);
    assertEquals(1, events[0].status.totalBytes);
    assertEquals(0, events[0].status.processedBytes);

    var lastEvent = events[events.length - 1];
    assertEquals('copy-progress', lastEvent.type);
    assertEquals('SUCCESS', lastEvent.reason);
    assertEquals(10, lastEvent.status.totalBytes);
    assertEquals(10, lastEvent.status.processedBytes);

    assertFalse(events.some(function(event) {
      return event.type === 'delete';
    }));

    assertTrue(events.some(function(event) {
      return event.type === 'entries-changed' &&
          event.entries[0].fullPath === '/test.zip';
    }));
  }), callback);

  fileOperationManager.zipSelection(
      fileSystem.entries['/'],
      [fileSystem.entries['/test.txt']]);
}
