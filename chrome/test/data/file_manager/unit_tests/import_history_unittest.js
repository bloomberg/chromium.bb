// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/** @const {string} */
var FILE_LAST_MODIFIED = new Date("Dec 4 1968").toString();

/** @const {string} */
var FILE_SIZE = 1234;

/** @const {string} */
var FILE_PATH = 'test/data';

/** @const {string} */
var GOOGLE_DRIVE = 'Google Drive';

/**
 * Space Cloud: Your source for interstellar cloud storage.
 * @const {string}
 */
var SPACE_CLOUD = 'Space Cloud';

/** @type {!TestFileSystem|undefined} */
var testFileSystem;

/** @type {!MockFileEntry|undefined} */
var testFileEntry;

/** @type {!RecordStorage|undefined} */
var storage;

/** @type {!Promise.<ImportHistory>|undefined} */
var historyLoader;

// Set up the test components.
function setUp() {
  testFileSystem = new TestFileSystem('abc-123');
  testFileEntry = new MockFileEntry(
      testFileSystem,
      FILE_PATH, {
        size: FILE_SIZE,
        modificationTime: FILE_LAST_MODIFIED
      });

  storage = new TestRecordStorage();
  historyLoader = ImportHistory.load(storage);
}

/**
 * In-memory test implementation of {@code RecordStorage}.
 *
 * @constructor
 * @implements {RecordStorage}
 */
var TestRecordStorage = function() {

  // Pre-populate the store with some "previously written" data <wink>.
  /** @private {!Array.<!Array.<string>>} */
  this.records_ = [
    [FILE_LAST_MODIFIED + '_' + FILE_SIZE, GOOGLE_DRIVE],
    ['99999_99999', SPACE_CLOUD]
  ];

  /**
   * @override
   * @this {TestRecordStorage}
   */
  this.readAll = function() {
    return Promise.resolve(this.records_);
  };

  /**
   * @override
   * @this {TestRecordStorage}
   */
  this.write = function(record) {
    this.records_.push(record);
    return Promise.resolve();
  };
};

/**
 * @return {!Promise.<RecordStorage>}
 */
function createRealStorage() {
  return new Promise(
      function(resolve, reject) {
        var onFilesystemReady = function(fileSystem) {
          fileSystem.root.getFile(
              'test.data',
              {
                create: true,
                exclusive: false
              },
              function(fileEntry) {
                resolve(new FileEntryRecordStorage(fileEntry));
              },
              reject);
        };

        window.webkitRequestFileSystem(
            TEMPORARY,
            1024 * 1024,
            onFilesystemReady,
            reject);
      });
}

function testRecordStorageRemembersPreviouslyWrittenRecords(callback) {
  createRealStorage()
      .then(
          function(storage) {
            storage.write(['abc', '123']).then(
                function() {
                  storage.readAll().then(
                      function(records) {
                        callback(/* error */ records.length != 1);
                      },
                      callback);
                });
            },
            callback)
      .catch(handleError.bind(callback));
}

function testHistoryWasImportedFalseForUnknownEntry(callback) {
  // TestRecordWriter is pre-configured with a Space Cloud entry
  // but not for this file.
  historyLoader.then(
      function(testHistory) {
        testHistory.wasImported(testFileEntry, SPACE_CLOUD).then(
          function(result) {
            callback(/* error */ result);
          });
      })
  .catch(handleError.bind(callback));
}

function testHistoryWasImportedTrueForKnownEntryLoadedFromStorage(callback) {
  // TestRecordWriter is pre-configured with this entry.
  historyLoader.then(
      function(testHistory) {
        testHistory.wasImported(testFileEntry, GOOGLE_DRIVE).then(
          function(result) {
            callback(/* error */ !result);
          });
      })
  .catch(handleError.bind(callback));
}

function testHistoryWasImportedTrueForKnownEntrySetAtRuntime(callback) {
  historyLoader.then(
      function(testHistory) {
        testHistory.markImported(testFileEntry, SPACE_CLOUD).then(
            function() {
              testHistory.wasImported(testFileEntry, SPACE_CLOUD).then(
                  function(result) {
                    callback(/* error */ !result);
                  });
            });
      })
  .catch(handleError.bind(callback));
}

/**
 * Shared error handler.
 * @param {function()} callback
 * @param {!Object} error
 */
function handleError(callback, error) {
  console.error(error.stack || error);
  callback(/* error */ true);
}
