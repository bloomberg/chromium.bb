// Copyright 2014 The Chromium Authors. All rights reserved.
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
      if (windows[windowId].outerWidth !== width ||
          windows[windowId].outerHeight !== height) {
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
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise.
 */
function testPromise(promise) {
  promise.then(function() {
    return new Promise(checkIfNoErrorsOccured);
  }).then(chrome.test.callbackPass(function() {
    // The callbacPass is necessary to avoid prematurely finishing tests.
    // Don't put chrome.test.succeed() here to avoid doubled success log.
  }), function(error) {
    chrome.test.fail(error.stack || error);
  });
};

/**
 * Sends a fake key down event.
 * @param {string} windowId Window ID.
 * @param {string} query Query for the target element.
 * @param {string} keyIdentifer Key identifier.
 * @param {boolean} ctrlKey Control key flag.
 * @return {Promise} Promise to be fulfilled or rejected depending on the
 *     result.
 */
function fakeKeyDown(windowId, query, keyIdentifer, ctrlKey) {
  var resultPromise = callRemoteTestUtil(
      'fakeKeyDown', windowId, [query, keyIdentifer, ctrlKey]);
  return resultPromise.then(function(result) {
    if (result)
      return true;
    else
      return Promise.reject('Fail to fake key down.');
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
 * Expecetd list of preset entries in fake test volumes. This should be in sync
 * with FakeTestVolume::PrepareTestEntries in the test harness.
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var BASIC_FAKE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.directoryA
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
 * @param {function(string)=} opt_callback Callback with the app id.
 * @return {Promise} Promise to be fulfilled after window creating.
 */
function openNewWindow(appState, initialRoot, opt_callback) {
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
                            opt_callback);
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
 * @param {function(string, Array.<Array.<string>>)=} opt_callback Callback with
 *     the window ID and with the file list.
 * @return {Promise} Promise to be fulfilled with window ID.
 */
function setupAndWaitUntilReady(appState, initialRoot, opt_callback) {
  var windowPromise = openNewWindow(appState, initialRoot);
  var localEntriesPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  var driveEntriesPromise = addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);
  var detailedTablePromise = windowPromise.then(function(windowId) {
    return waitForElement(windowId, '#detail-table').then(function() {
      // Wait until the elements are loaded in the table.
      return waitForFileListChange(windowId, 0);
    });
  });

  if (opt_callback)
    opt_callback = chrome.test.callbackPass(opt_callback);

  return Promise.all([
    windowPromise,
    localEntriesPromise,
    driveEntriesPromise,
    detailedTablePromise
  ]).then(function(results) {
    if (opt_callback)
      opt_callback(results[0], results[3]);
    return results[0];
  }).catch(function(e) {
    chrome.test.fail(e.stack || e);
  });
}

/**
 * Verifies if there are no Javascript errors in any of the app windows.
 * @param {function()} Completion callback.
 */
function checkIfNoErrorsOccured(callback) {
  callRemoteTestUtil('getErrorCount', null, [], function(count) {
    chrome.test.assertEq(0, count, 'The error count is not 0.');
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
      var targetTest = testcase[testCaseName];
      if (!targetTest) {
        chrome.test.fail(testCaseName + ' is not found.');
        return;
      }
      // Specify the name of test to the test system.
      targetTest.generatedName = testCaseName;
      chrome.test.runTests([targetTest]);
    }
  ];
  steps.shift()();
});
