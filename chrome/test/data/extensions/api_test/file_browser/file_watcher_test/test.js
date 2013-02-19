// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test component extension that tests fileBrowserPrivate file watch api.
 * The extension adds file watch on set of entries and performs set of file
 * system operations that should trigger onDirectoryChanged events for the
 * watched entries. On file system operations is performed per a test function.
*/

/**
 * Helper class to observe the events triggered during a file system operation
 * performed during a single test function.
 * The received events are verified against the list of expected events, but
 * only after the file system operation is done. If an event is received before
 * an operation is done, it is added to the event queue that will be verified
 * after the operation. chrome.test.succeed is called when all the expected
 * events are received and verified.
 *
 * @constructor
 */
function TestEventListener() {
  /**
   * Maps expectedEvent.directoryUrl ->
   *     {expectedEvent.eventType, expectedEvent.changeType}
   *
   * Set of events that are expected to be triggered during the test. Each
   * object property represents one expected event.
   *
   * @type {Object.<string, Object>}
   * @private
   */
  this.expectedEvents_ = {};

  /**
   * List of fileBrowserPrivate.onDirectoryChanged events received before file
   * system operation was done.
   *
   * @type {Array.<Object>}
   * @private
   */
  this.eventQueue_ = [];

  /**
   * Whether the test listener is done. When set, all further |onSuccess_| and
   * |onError| calls are ignored.
   *
   * @type {boolean}
   * @private
   */
  this.done_ = false;

  /**
   * An entry returned by the test file system operation.
   *
   * @type {Entry}
   * @private
   */
  this.receivedEntry_ = null;

  /**
   * The listener to the fileBrowserPrivate.onDirectoryChanged.
   *
   * @type {function(Object)}
   * @private
   */
  this.eventListener_ = this.onDirectoryChanged_.bind(this);
}

TestEventListener.prototype = {
  /**
   * Starts listening for the onDirectoryChanged events.
   */
  start: function() {
    chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
        this.eventListener_);
  },

  /**
   * Adds expectation for an event that should be encountered during the test.
   *
   * @param {string} directoryUrl The event's directory URL parameter.
   * @param {string} eventType The event't type.
   * @param {string} changeType The change type for the entry specified in
   *     event.changedEntries[0].
   */
  addExpectedEvent: function(directoryUrl, eventType, changeType) {
    this.expectedEvents_[directoryUrl] = {
        eventType: eventType,
        changeType: changeType,
    };
  },

  /**
   * Called by a test when the file system operation performed in the test
   * succeeds.
   *
   * @param {Entry} entry The entry returned by the file system operation.
   */
  onFileSystemOperation: function(entry) {
    this.receivedEntry_ = entry;
    this.eventQueue_.forEach(function(event) {
      this.verifyReceivedEvent_(event);
    }.bind(this));
  },

  /**
   * Called when the test encounters an error. Does cleanup and ends the test
   * with failure. Further |onError| and |onSuccess| calls will be ignored.
   *
   * @param {string} message An error message.
   */
  onError: function(message) {
    if (this.done_)
      return;
    this.done_ = true;

    chrome.fileBrowserPrivate.onDirectoryChanged.removeListener(
        this.eventListener_);
    chrome.test.fail(message);
  },

  /**
   * Called when the test succeeds. Does cleanup and calls chrome.test.succeed.
   * Further |onError| and |onSuccess| calls will be ignored.
   *
   * @private
   */
  onSuccess_: function() {
    if (this.done_)
      return;
    this.done_ = true;

    chrome.fileBrowserPrivate.onDirectoryChanged.removeListener(
        this.eventListener_);
    chrome.test.succeed();
  },

  /**
   * onDirectoryChanged event listener.
   * If the test file system operation is done, verifies the event, otherwise
   * it adds the event to |eventQueue_|. The events from |eventQueue_| will be
   * verified once the file system operation is done.
   *
   * @param {Object} event chrome.fileBrowserPrivate.onDirectoryChanged event.
   * @private
   */
  onDirectoryChanged_: function(event) {
    if (this.receivedEntry_) {
      this.verifyReceivedEvent_(event);
    } else {
      this.eventQueue_.push(event);
    }
  },

  /**
   * Verifies a received event.
   * It checks that there is an expected event for |event.directoryUrl|.
   * If there is, the event is removed from the set of expected events.
   * It verifies that the recived event matches the expected event parameters.
   * If the received event was the last expected event, onSuccess_ is called.
   *
   * @param {Object} event chrome.fileBrowserPrivate.onDirectoryChanged event.
   * @private
   */
  verifyReceivedEvent_: function(event) {
    var expectedEvent = this.expectedEvents_[event.directoryUrl];
    if (!expectedEvent) {
      this.onError('Event with unexpected dir url: ' + event.directoryUrl);
      return;
    }

    delete this.expectedEvents_[event.directoryUrl];

    if (expectedEvent.eventType != event.eventType) {
      this.onError('Unexpected event type for directoryUrl: ' +
                   event.directoryUrl + '.\n' +
                   'Expected "' + expectedEvent.eventType + '"\n' +
                   'Got: "' + event.eventType + '"');
      return;
    }

    // TODO(mtomasz): When http://crbug.com/157834 is fixed add the expected
    // change type and the received entry's url to |expectedChangedEntries|.
    // Until the bug is fixed, the events carry an empty list of changed
    // entries.

    // Array of objects with keys |changeType| and  |fileUrl|.
    var expectedChangedEntries = [];

    if (!this.verifyChangedEntries_(expectedChangedEntries,
                                    event.changedEntries)) {
      this.onError('Unexpected changed entries for directoryUrl: "' +
                   event.directoryUrl +
                   '".\n Expected: ' + JSON.stringify(expectedChangedEntries) +
                   '.\n Got: ' + JSON.stringify(event.changedEntries));
      return;
    }

    if (Object.keys(this.expectedEvents_).length == 0)
      this.onSuccess_();
  },

  /**
   * Checks the list of changedEntries received in directoryChanged event is the
   * same as the expected one.
   * It checks that the lists have the same length and that the changed entries
   * have the same fileUrl and changeType parameters.
   *
   * @param {Array.<Object>} expected Expected list of changed entries.
   * @param {Array.<Object>} expected List of received changed entries.
   * @private
   */
  verifyChangedEntries_: function(expected, received) {
    if (expected.length != received.length)
      return false;

    // Until http://crbug.com/157834 is fixed, |expected| will be empty, and
    // this loop no-op.
    for (var i = 0; i < expected.length; i++) {
      if (Object.keys(expected[i]).length != Object.keys(received[i]).length)
        return false;
      if (expected[i].fileUrl != received[i].fileUrl)
        return false;
      if (expected[i].changeType != received[i].changeType)
        return false;
    }

    return true;
  }
}

/**
 * Initializes test parameters:
 * - Gets local file system.
 * - Gets the test mount point.
 * - Adds the entries that will be watched during the test.
 *
 * @param {function(Object, string)} callback The function called when the test
 *    parameters are initialized. Called with testParams object and an error
 *    message string. The error message should be ignored if testParams are
 *    valid.
 */
function initTests(callback) {
  var testParams = {
    /**
     * Whether the test parameters are valid.
     * @type {boolean}
     */
    valid: false,
    /**
     * Mount point root directory entry.
     * @type {DirectoryEntry}
     */
    mountPoint: null,
    // TODO(tbarzic) : We should not need to have this. The watch api should
    // have the same behavior for local and drive file system.
    isOnDrive: false,
    /**
     * Set of entries that are being watched during the tests.
     * @type {Object.<Entry>}
     */
    entries: {}
  };

  // Get the file system.
  chrome.fileBrowserPrivate.requestLocalFileSystem(function(fileSystem) {
    if(!fileSystem) {
      callback(testParams, 'Failed to get file system,');
      return;
    }

    var possibleMountPoints = ['local/', 'drive/'];

    function tryNextMountPoint() {
      if (possibleMountPoints.length == 0) {
        callback(testParams, 'No mount point found.');
        return;
      }

      var mountPointPath = possibleMountPoints.shift();

      // Try to get the current mount point path. On failure,
      // |tryNextMountPoint| is called.
      fileSystem.root.getDirectory(
          mountPointPath, {},
          function(mountPoint) {
            // The test mount point has been found. Get all the entries that
            // will be watched during the test.
            testParams.mountPoint = mountPoint;
            testParams.isOnDrive = mountPointPath == 'drive/';

            var testWatchEntries = [
              {name: 'file', path: 'test_dir/test_file.xul', type: 'file'},
              {name: 'dir', path: 'test_dir/', type: 'dir'},
              {name: 'subdir', path: 'test_dir/subdir', type: 'dir'},
            ];

            // Gets the first entry in |testWatchEntries| list.
            function getNextEntry() {
              // If the list is empty, the test has been successfully
              // initialized, so call callback.
              if (testWatchEntries.length == 0) {
                testParams.valid = true;
                callback(testParams, 'Success.');
                return;
              }

              var testEntry = testWatchEntries.shift();

              var getFunction = null;
              if (testEntry.type == 'file') {
                getFunction = mountPoint.getFile.bind(mountPoint);
              } else {
                getFunction = mountPoint.getDirectory.bind(mountPoint);
              }

              getFunction(testEntry.path, {},
                  function(entry) {
                    testParams.entries[testEntry.name] = entry;
                    getNextEntry();
                  },
                  callback.bind(null, testParams,
                      'Unable to get entry: \'' + testEntry.path + '\'.'));
            };

            // Trigger getting the watched entries.
            getNextEntry();

          },
          tryNextMountPoint);
    };

    // Trigger getting the test mount point.
    tryNextMountPoint();
  })
};

// Starts the test.
initTests(function(testParams, errorMessage) {
  if (!testParams.valid) {
    chrome.test.notifyFail('Failed to initialize tests: ' + errorMessage);
    return;
  }

  chrome.test.runTests([
    function addFileWatch() {
      chrome.fileBrowserPrivate.addFileWatch(
          testParams.entries.file.toURL(),
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function addSubdirWatch() {
      chrome.fileBrowserPrivate.addFileWatch(
          testParams.entries.subdir.toURL(),
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function addDirWatch() {
      chrome.fileBrowserPrivate.addFileWatch(
          testParams.entries.dir.toURL(),
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    // Test that onDirectoryChanged is triggerred when a directory in a watched
    // directory is created.
    function onCreateDir() {
      var testEventListener = new TestEventListener();
      testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                         'changed', 'added');
      testEventListener.start();

      testParams.mountPoint.getDirectory(
          'test_dir/subdir/subsubdir', {create: true, exclusive: true},
          testEventListener.onFileSystemOperation.bind(testEventListener),
          testEventListener.onError.bind(testEventListener,
                                         'Failed to create directory.'));
    },

    // Test that onDirectoryChanged is triggerred when a file in a watched
    // directory is created.
    function onCreateFile() {
      var testEventListener = new TestEventListener();
      testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                         'changed', 'added');
      testEventListener.start();

      testParams.mountPoint.getFile(
          'test_dir/subdir/file', {create: true, exclusive: true},
          testEventListener.onFileSystemOperation.bind(testEventListener),
          testEventListener.onError.bind(testEventListener,
                                         'Failed to create file.'));
    },

    // Test that onDirectoryChanged is triggerred when a file in a watched
    // directory is renamed.
    function onFileUpdated() {
      var testEventListener = new TestEventListener();
      testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                         'changed', 'updated');

      testEventListener.start();

      testParams.mountPoint.getFile(
          'test_dir/subdir/file', {},
          function(entry) {
            entry.moveTo(testParams.entries.subdir, 'renamed',
                testEventListener.onFileSystemOperation.bind(testEventListener),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to rename the file.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get file.'));
    },

    // Test that onDirectoryChanged is triggerred when a file in a watched
    // directory is deleted.
    function onDeleteFile() {
      var testEventListener = new TestEventListener();
      testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                         'changed', 'deleted');
      testEventListener.start();

      testParams.mountPoint.getFile(
          'test_dir/subdir/renamed', {},
          function(entry) {
            entry.remove(
                testEventListener.onFileSystemOperation.bind(testEventListener,
                                                             entry),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to remove the file.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get the file.'));
    },

    // Test that onDirectoryChanged is triggerred when a watched file in a
    // watched directory is deleted.
    // The behaviour is different for drive and local mount points. On drive,
    // there will be no event for the watched file.
    function onDeleteWatchedFile() {
      var testEventListener = new TestEventListener();
       testEventListener.addExpectedEvent(testParams.entries.dir.toURL(),
                                          'changed', 'deleted');
      if (!testParams.isOnDrive) {
        testEventListener.addExpectedEvent(testParams.entries.file.toURL(),
                                           'changed', 'deleted');
      }
      testEventListener.start();

      testParams.mountPoint.getFile(
          'test_dir/test_file.xul', {},
          function(entry) {
            entry.remove(
                testEventListener.onFileSystemOperation.bind(testEventListener,
                                                             entry),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to remove the file.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get the file.'));
    },

    // Test that onDirectoryChanged is triggerred when a directory in a
    // watched directory is deleted.
    function onDeleteDir() {
      var testEventListener = new TestEventListener();
      testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                         'changed', 'deleted');
      testEventListener.start();

      testParams.mountPoint.getDirectory(
          'test_dir/subdir/subsubdir', {},
          function(entry) {
            entry.removeRecursively(
                testEventListener.onFileSystemOperation.bind(testEventListener,
                                                             entry),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to remove the dir.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get the dir.'));
    },

    // Test that onDirectoryChanged is triggerred when a watched directory in a
    // watched directory is deleted.
    // The behaviour is different for drive and local mount points. On drive,
    // there will be no event for the deleted directory.
    function onDeleteWatchedDir() {
      var testEventListener = new TestEventListener();
      if (!testParams.isOnDrive) {
        testEventListener.addExpectedEvent(testParams.entries.subdir.toURL(),
                                           'changed', 'deleted');
      }
      testEventListener.addExpectedEvent(testParams.entries.dir.toURL(),
                                         'changed', 'deleted');
      testEventListener.start();

      testParams.mountPoint.getDirectory('test_dir/subdir', {},
          function(entry) {
            entry.removeRecursively(
                testEventListener.onFileSystemOperation.bind(testEventListener,
                                                             entry),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to remove the dir.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get the dir.'));
    },

    function removeFileWatch() {
      chrome.fileBrowserPrivate.removeFileWatch(
          testParams.entries.file.toURL(),
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function removeDirWatch() {
      chrome.fileBrowserPrivate.removeFileWatch(
          testParams.entries.dir.toURL(),
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    }

    // The watch for subdir entry is intentionally not removed to simulate the
    // case when File Manager does not remove it either (e.g. if it's opened
    // during shutdown).
  ]);
});
