// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of Files.app.
 * @type {string}
 * @const
 */
var FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Calls a remote test util in Files.app's extension. See: test_util.js.
 *
 * @param {string} func Function name.
 * @param {?string} appId Target window's App ID or null for functions
 *     not requiring a window.
 * @param {Array.<*>} args Array of arguments.
 * @param {function(*)=} opt_callback Callback handling the function's result.
 * @return {Promise} Promise to be fulfilled with the result of the remote
 *     utility.
 */
function callRemoteTestUtil(func, appId, args, opt_callback) {
  return new Promise(function(onFulfilled) {
    chrome.runtime.sendMessage(
        FILE_MANAGER_EXTENSIONS_ID, {
          func: func,
          appId: appId,
          args: args
        },
        function() {
          if (opt_callback)
            opt_callback.apply(null, arguments);
          onFulfilled(arguments[0]);
        });
  });
}

/**
 * Returns promise to be fulfilled after the given milliseconds.
 * @param {number} time Time in milliseconds.
 */
function wait(time) {
  return new Promise(function(callback) {
    setTimeout(callback, time);
  });
}

/**
 * Interval milliseconds between checks of repeatUntil.
 * @type {number}
 * @const
 */
var REPEAT_UNTIL_INTERVAL = 200;

/**
 * Interval milliseconds between log output of repeatUntil.
 * @type {number}
 * @const
 */
var LOG_INTERVAL = 3000;

/**
 * Returns a pending marker. See also the repeatUntil function.
 * @param {string} message Pending reason including %s, %d, or %j markers. %j
 *     format an object as JSON.
 * @param {Array.<*>} var_args Values to be assigined to %x markers.
 * @return {Object} Object which returns true for the expression: obj instanceof
 *     pending.
 */
function pending(message, var_args) {
  var index = 1;
  var args = arguments;
  var formattedMessage = message.replace(/%[sdj]/g, function(pattern) {
    var arg = args[index++];
    switch(pattern) {
      case '%s': return String(arg);
      case '%d': return Number(arg);
      case '%j': return JSON.stringify(arg);
      default: return pattern;
    }
  });
  var pendingMarker = Object.create(pending.prototype);
  pendingMarker.message = formattedMessage;
  return pendingMarker;
};

/**
 * Waits until the checkFunction returns a value but a pending marker.
 * @param {function():*} checkFunction Function to check a condition. It can
 *     return a pending marker created by a pending function.
 * @return {Promise} Promise to be fulfilled with the return value of
 *     checkFunction when the checkFunction reutrns a value but a pending
 *     marker.
 */
function repeatUntil(checkFunction) {
  var logTime = Date.now() + LOG_INTERVAL;
  var step = function() {
    return checkFunction().then(function(result) {
      if (result instanceof pending) {
        if (Date.now() > logTime) {
          console.log(result.message);
          logTime += LOG_INTERVAL;
        }
        return wait(REPEAT_UNTIL_INTERVAL).then(step);
      } else {
        return result;
      }
    });
  };
  return step();
};

/**
 * Waits until a window having the given ID prefix appears.
 * @param {string} windowIdPrefix ID prefix of the requested window.
 * @return {Promise} promise Promise to be fulfilled with a found window's ID.
 */
function waitForWindow(windowIdPrefix) {
  return repeatUntil(function() {
    return callRemoteTestUtil('getWindows', null, []).then(function(windows) {
      for (var id in windows) {
        if (id.indexOf(windowIdPrefix) === 0)
          return id;
      }
      return pending('Window with the prefix %s is not found.', windowIdPrefix);
    });
  });
}

/**
 * Closes a window and waits until the window is closed.
 *
 * @param {string} windowId ID of the window to close.
 * @return {Promise} promise Promise to be fulfilled with the result (true:
 *     success, false: failed).
 */
function closeWindowAndWait(windowId) {
  // Closes the window.
  return callRemoteTestUtil('closeWindow', null, [windowId]).then(
      function(result) {
        // Returns false when the closing is failed.
        if (!result)
          return false;

        return repeatUntil(function() {
          return callRemoteTestUtil('getWindows', null, []).then(
              function(windows) {
                for (var id in windows) {
                  if (id === windowId) {
                    // Window is still available. Continues waiting.
                    return pending('Window with the prefix %s is not found.',
                                   windowId);
                  }
                }
                // Window is not available. Closing is done successfully.
                return true;
              }
          );
        });
      }
  );
}

/**
 * Waits until the window turns to the given size.
 * @param {string} windowId Target window ID.
 * @param {number} width Requested width in pixels.
 * @param {number} height Requested height in pixels.
 */
function waitForWindowGeometry(windowId, width, height) {
  return repeatUntil(function() {
    return callRemoteTestUtil('getWindows', null, []).then(function(windows) {
      if (!windows[windowId])
        return pending('Window %s is not found.', windowId);
      if (windows[windowId].innerWidth !== width ||
          windows[windowId].innerHeight !== height) {
        return pending('Expected window size is %j, but it is %j',
                       {width: width, height: height},
                       windows[windowId]);
      }
    });
  });
}

/**
 * Waits for the specified element appearing in the DOM.
 * @param {string} windowId Target window ID.
 * @param {string} query Query string for the element.
 * @param {string=} opt_iframeQuery Query string for the iframe containing the
 *     element.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
function waitForElement(windowId, query, opt_iframeQuery) {
  return repeatUntil(function() {
    return callRemoteTestUtil(
        'queryAllElements',
        windowId,
        [query, opt_iframeQuery]
    ).then(function(elements) {
      if (elements.length > 0)
        return elements[0];
      else
        return pending(
            'Element %s (maybe in iframe %s) is not found.',
            query,
            opt_iframeQuery);
    });
  });
}

/**
 * Waits for the specified element leaving from the DOM.
 * @param {string} windowId Target window ID.
 * @param {string} query Query string for the element.
 * @param {string=} opt_iframeQuery Query string for the iframe containing the
 *     element.
 * @return {Promise} Promise to be fulfilled when the element is lost.
 */
function waitForElementLost(windowId, query, opt_iframeQuery) {
  return repeatUntil(function() {
    return callRemoteTestUtil(
        'queryAllElements',
        windowId,
        [query, opt_iframeQuery]
    ).then(function(elements) {
      if (elements.length > 0)
        return pending('Elements %j is still exists.', elements);
      return true;
    });
  });
}

/**
 * Waits for the specified element appearing in the DOM.
 * @param {string} windowId Target window ID.
 * @param {string} query Query string for the element.
 * @param {string=} opt_iframeQuery Query string for the iframe containing the
 *     element.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
function verifyElenmentTextContent(windowId, query, text, opt_iframeQuery) {
  return callRemoteTestUtil(
      'queryAllElements',
      windowId,
      [query, opt_iframeQuery]
  ).then(function(elements) {
    if (elements.length > 0) {
      return (elements[0].textContent === text);
    }
    return false;
  });
}

/**
/**
 * Waits for the file list turns to the given contents.
 * @param {string} windowId Target window ID.
 * @param {Array.<Array.<string>>} expected Expected contents of file list.
 * @param {{orderCheck:boolean=, ignoreLastModifiedTime:boolean=}=} opt_options
 *     Options of the comparison. If orderCheck is true, it also compares the
 *     order of files. If ignoreLastModifiedTime is true, it compares the file
 *     without its last modified time.
 * @return {Promise} Promise to be fulfilled when the file list turns to the
 *     given contents.
 */
function waitForFiles(windowId, expected, opt_options) {
  var options = opt_options || {};
  return repeatUntil(function() {
    return callRemoteTestUtil(
        'getFileList', windowId, []).then(function(files) {
      if (!options.orderCheck) {
        files.sort();
        expected.sort();
      }
      for (var i = 0; i < Math.min(files.length, expected.length); i++) {
        if (options.ignoreFileSize) {
          files[i][1] = '';
          expected[i][1] = '';
        }
        if (options.ignoreLastModifiedTime) {
          files[i][3] = '';
          expected[i][3] = '';
        }
      }
      if (!chrome.test.checkDeepEq(expected, files)) {
        return pending('waitForFiles: expected: %j actual %j.',
                       expected,
                       files);
      }
    });
  });
}

/**
 * Waits until the number of files in the file list is changed from the given
 * number.
 * TODO(hirono): Remove the function.
 *
 * @param {string} windowId Target window ID.
 * @param {number} lengthBefore Number of items visible before.
 * @return {Promise} Promise to be fulfilled with the contents of files.
 */
function waitForFileListChange(windowId, lengthBefore) {
  return repeatUntil(function() {
    return callRemoteTestUtil(
        'getFileList', windowId, []).then(function(files) {
      files.sort();
      var notReadyRows = files.filter(function(row) {
        return row.filter(function(cell) { return cell == '...'; }).length;
      });
      if (notReadyRows.length === 0 &&
          files.length !== lengthBefore &&
          files.length !== 0) {
        return files;
      } else {
        return pending('The number of file is %d. Not changed.', lengthBefore);
      }
    });
  });
};

/**
 * Waits until the given taskId appears in the executed task list.
 * @param {string} windowId Target window ID.
 * @param {string} taskId Task ID to watch.
 * @return {Promise} Promise to be fulfilled when the task appears in the
 *     executed task list.
 */
function waitUntilTaskExecutes(windowId, taskId) {
  return repeatUntil(function() {
    return callRemoteTestUtil('getExecutedTasks', windowId, []).
        then(function(executedTasks) {
          if (executedTasks.indexOf(taskId) === -1)
            return pending('Executed task is %j', executedTasks);
        });
  });
}

/**
 * Sends fake key down event.
 * @param {string} windowId Window ID.
 * @param {string} query Query for target element.
 * @param {string} keyIdentifer Key identifier.
 * @param {boolean} ctrlKey Control key flag.
 * @return {Promise} Promise to be fulfilled or rejected depending on the
 *     result.
 */
function fakeKeyDown(windowId, query, keyIdentifer, ctrlKey) {
  return new Promise(function(fulfill, reject) {
    callRemoteTestUtil('fakeKeyDown',
                       windowId,
                       [query, keyIdentifer, ctrlKey],
                       function(result) {
                         if (result)
                           fulfill();
                         else
                           reject(new Error('Fail to fake key down.'));
                       });
  });
}

/**
 * Executes a sequence of test steps.
 * @constructor
 */
function StepsRunner() {
  /**
   * List of steps.
   * @type {Array.<function>}
   * @private
   */
  this.steps_ = [];
}

/**
 * Creates a StepsRunner instance and runs the passed steps.
 */
StepsRunner.run = function(steps) {
  var stepsRunner = new StepsRunner();
  stepsRunner.run_(steps);
};

StepsRunner.prototype = {
  /**
   * @return {function} The next closure.
   */
  get next() {
    return this.steps_[0];
  }
};

/**
 * Runs a sequence of the added test steps.
 * @type {Array.<function>} List of the sequential steps.
 */
StepsRunner.prototype.run_ = function(steps) {
  this.steps_ = steps.slice(0);

  // An extra step which acts as an empty callback for optional asynchronous
  // calls in the last provided step.
  this.steps_.push(function() {});

  this.steps_ = this.steps_.map(function(f) {
    return chrome.test.callbackPass(function() {
      this.steps_.shift();
      f.apply(this, arguments);
    }.bind(this));
  }.bind(this));

  this.next();
};

/**
 * Adds the givin entries to the target volume(s).
 * @param {Array.<string>} volumeNames Names of target volumes.
 * @param {Array.<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)} callback Callback function to be passed the result
 *     of function. The argument is true on success.
 */
function addEntries(volumeNames, entries, callback) {
  if (volumeNames.length == 0) {
    callback(true);
    return;
  }
  chrome.test.sendMessage(JSON.stringify({
    name: 'addEntries',
    volume: volumeNames.shift(),
    entries: entries
  }), chrome.test.callbackPass(function(result) {
    if (result == "onEntryAdded")
      addEntries(volumeNames, entries, callback);
    else
      callback(false);
  }));
};

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory'
});

/**
 * @enum {string}
 * @const
 */
var SharedOption = Object.freeze({
  NONE: 'none',
  SHARED: 'shared'
});

/**
 * @enum {string}
 */
var RootPath = Object.seal({
  DOWNLOADS: '/must-be-filled-in-test-setup',
  DRIVE: '/must-be-filled-in-test-setup',
});

/**
 * File system entry information for tests.
 *
 * @param {EntryType} type Entry type.
 * @param {string} sourceFileName Source file name that provides file contents.
 * @param {string} targetName Name of entry on the test file system.
 * @param {string} mimeType Mime type.
 * @param {SharedOption} sharedOption Shared option.
 * @param {string} lastModifiedTime Last modified time as a text to be shown in
 *     the last modified column.
 * @param {string} nameText File name to be shown in the name column.
 * @param {string} sizeText Size text to be shown in the size column.
 * @param {string} typeText Type name to be shown in the type column.
 * @constructor
 */
function TestEntryInfo(type,
                       sourceFileName,
                       targetPath,
                       mimeType,
                       sharedOption,
                       lastModifiedTime,
                       nameText,
                       sizeText,
                       typeText) {
  this.type = type;
  this.sourceFileName = sourceFileName || '';
  this.targetPath = targetPath;
  this.mimeType = mimeType || '';
  this.sharedOption = sharedOption;
  this.lastModifiedTime = lastModifiedTime;
  this.nameText = nameText;
  this.sizeText = sizeText;
  this.typeText = typeText;
  Object.freeze(this);
};

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
};

/**
 * Obtains a expected row contents of the file in the file list.
 */
TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

/**
 * Filesystem entries used by the test cases.
 * @type {Object.<string, TestEntryInfo>}
 * @const
 */
var ENTRIES = {
  hello: new TestEntryInfo(
      EntryType.FILE, 'text.txt', 'hello.txt',
      'text/plain', SharedOption.NONE, 'Sep 4, 1998 12:34 PM',
      'hello.txt', '51 bytes', 'Plain text'),

  world: new TestEntryInfo(
      EntryType.FILE, 'video.ogv', 'world.ogv',
      'text/plain', SharedOption.NONE, 'Jul 4, 2012 10:35 AM',
      'world.ogv', '59 KB', 'OGG video'),

  unsupported: new TestEntryInfo(
      EntryType.FILE, 'random.bin', 'unsupported.foo',
      'application/x-foo', SharedOption.NONE, 'Jul 4, 2012 10:36 AM',
      'unsupported.foo', '8 KB', 'FOO file'),

  desktop: new TestEntryInfo(
      EntryType.FILE, 'image.png', 'My Desktop Background.png',
      'text/plain', SharedOption.NONE, 'Jan 18, 2038 1:02 AM',
      'My Desktop Background.png', '272 bytes', 'PNG image'),

  beautiful: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'Beautiful Song.ogg',
      'text/plain', SharedOption.NONE, 'Nov 12, 2086 12:00 PM',
      'Beautiful Song.ogg', '14 KB', 'OGG audio'),

  photos: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'photos',
      null, SharedOption.NONE, 'Jan 1, 1980 11:59 PM',
      'photos', '--', 'Folder'),

  testDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Document',
      'application/vnd.google-apps.document',
      SharedOption.NONE, 'Apr 10, 2013 4:20 PM',
      'Test Document.gdoc', '--', 'Google document'),

  testSharedDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Shared Document',
      'application/vnd.google-apps.document',
      SharedOption.SHARED, 'Mar 20, 2013 10:40 PM',
      'Test Shared Document.gdoc', '--', 'Google document'),

  newlyAdded: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'newly added file.ogg',
      'audio/ogg', SharedOption.NONE, 'Sep 4, 1998 12:00 AM',
      'newly added file.ogg', '14 KB', 'OGG audio'),

  directoryA: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'A', '--', 'Folder'),

  directoryB: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'B', '--', 'Folder'),

  directoryC: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B/C',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'C', '--', 'Folder'),

  zipArchive: new TestEntryInfo(
      EntryType.FILE, 'archive.zip', 'archive.zip',
      'application/x-zip', SharedOption.NONE, 'Jan 1, 2014 1:00 AM',
      'archive.zip', '533 bytes', 'Zip archive')
};

/**
 * Basic entry set for the local volume.
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos
];

/**
 * Basic entry set for the drive volume.
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

var NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var RECENT_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var SHARED_WITH_ME_ENTRY_SET = [
  ENTRIES.testSharedDocument
];

/**
 * Opens a Files.app's main window.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {Object} appState App state to be passed with on opening Files.app.
 *     Can be null.
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {function(string)} Callback with the app id.
 * @return {Promise} Promise to be fulfilled after window creating.
 */
function openNewWindow(appState, initialRoot, callback) {
  var appId;

  // TODO(mtomasz): Migrate from full paths to a pair of a volumeId and a
  // relative path. To compose the URL communicate via messages with
  // file_manager_browser_test.cc.
  var processedAppState = appState || {};
  if (initialRoot) {
    processedAppState.currentDirectoryURL =
        'filesystem:chrome-extension://' + FILE_MANAGER_EXTENSIONS_ID +
        '/external' + initialRoot;
  }

  return callRemoteTestUtil('openMainWindow',
                            null,
                            [processedAppState],
                            callback);
}

/**
 * Opens a Files.app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 *
 * TODO(hirono): Add parameters to specify the entry set to be prepared.
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {Object} appState App state to be passed with on opening Files.app.
 *     Can be null.
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {function(string, Array.<Array.<string>>)} Callback with the app id
 *     and with the file list.
 */
function setupAndWaitUntilReady(appState, initialRoot, callback) {
  var appId;

  StepsRunner.run([
    function() {
      openNewWindow(appState, initialRoot, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      addEntries(['local'], BASIC_LOCAL_ENTRY_SET, this.next);
    },
    function(success) {
      chrome.test.assertTrue(success);
      addEntries(['drive'], BASIC_DRIVE_ENTRY_SET, this.next);
    },
    function(success) {
      chrome.test.assertTrue(success);
      waitForElement(appId, '#detail-table').then(this.next);
    },
    function(success) {
      waitForFileListChange(appId, 0).then(this.next);
    },
    function(fileList) {
      callback(appId, fileList);
      this.next();
    }
  ]);
}

/**
 * Verifies if there are no Javascript errors in any of the app windows.
 * @param {function()} Completion callback.
 */
function checkIfNoErrorsOccured(callback) {
  callRemoteTestUtil('getErrorCount', null, [], function(count) {
    chrome.test.assertEq(0, count);
    callback();
  });
}

/**
 * Returns the name of the given file list entry.
 * @param {Array.<string>} file An entry in a file list.
 * @return {string} Name of the file.
 */
function getFileName(fileListEntry) {
  return fileListEntry[0];
}

/**
 * Returns the size of the given file list entry.
 * @param {Array.<string>} An entry in a file list.
 * @return {string} Size of the file.
 */
function getFileSize(fileListEntry) {
  return fileListEntry[1];
}

/**
 * Returns the type of the given file list entry.
 * @param {Array.<string>} An entry in a file list.
 * @return {string} Type of the file.
 */
function getFileType(fileListEntry) {
  return fileListEntry[2];
}

/**
 * Namespace for test cases.
 */
var testcase = {};

// Ensure the test cases are loaded.
window.addEventListener('load', function() {
  var steps = [
    // Check for the guest mode.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'isInGuestMode'}), steps.shift());
    },
    // Obtain the test case name.
    function(result) {
      if (JSON.parse(result) != chrome.extension.inIncognitoContext)
        return;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getRootPaths'}), steps.shift());
    },
    // Obtain the root entry paths.
    function(result) {
      var roots = JSON.parse(result);
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.DRIVE = roots.drive;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getTestName'}), steps.shift());
    },
    // Run the test case.
    function(testCaseName) {
      if (!testcase[testCaseName]) {
        chrome.test.runTests([function() {
          chrome.test.fail(testCaseName + ' is not found.');
        }]);
        return;
      }
      chrome.test.runTests([testcase[testCaseName]]);
    }
  ];
  steps.shift()();
});
