// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs test to verify that file browser tasks can successfully be executed.
 * The test does the following:
 * - Open external filesystem.
 * - Get the test mount point root. The root is determined by probing exitents
 *   of root directories 'local' or 'drive'.
 * - Get files 'test_dir/test_file.xul' and 'test_dir/test_file.tiff' on the
 *   test mount point.
 *   Chrome part of the test should ensure that these actually exist.
 * - For each of the files do following:
 *   - Get registered file tasks.
 *   - Verify there is exactly one registered task (Chrome part of the test
 *     should ensure this: it should launch an extension that has exactly one
 *     handler for each of files).
 *   - Execute the task.
 *
 * If there is a failure in any of these steps, the extension reports it back to
 * Chrome, otherwise it does nothing. The responsibility to report test result
 * is given to file task handler.
 */

/**
 * Files for which the file browser handlers will be executed by the extension.
 * Each test case contains the file's file path and mimeType that will be given
 * to chrome.fileBrowserPrivate.executeTask for the file.
 *
 * @type {Array.<Object.<string, string>>}
 */
var kTestCases = [
    {
      path: 'test_dir/test_file.xul',
      mimeType: ''
    },
    {
      path: 'test_dir/test_file.tiff',
      mimeType: ''
    }
];

// Starts the test extension.
function run() {
  /**
   * Test cases after the file path has been resolved to FileEntry. Each
   * resolved test case contains the resolved FileEntry object and mimeType.
   *
   * @type Array.<Object.<FileEntry, string>>
   */

  var resolvedTestCases = [];
  /**
   * List of tasks found for a testCase. Each object contains the found task id
   * and file URL for which the task should be executed.
   *
   * @type {Array.<Object.<string, string>>}
   */
  var foundTasks = [];

  /**
   * Whether the test extension has done its job. When done is set |onError|
   * calls will be ignored.
   * @type {boolean}
   */
  var done = false;

  /**
   * Function called when an error is encountered. It sends test failure
   * notification.
   *
   * @param {string} errorMessage The error message to be send.
   */
  function onError(errorMessage) {
    if (done)
      return;

    chrome.test.notifyFail(errorMessage);
    // there should be at most one notifyFail call.
    done = true;
  }

  /**
   * Callback to chrome.fileBrowserPrivate.executeTask. Verifies the function
   * succeeded.
   *
   * @param {string} url The url of file for which the handler was executed.
   * @param {boolean} success Whether the function succeeded.
   */
  function onExecuteTask(url, success) {
    if (!success)
      onError('Failed to execute task for ' + url);
  }

  /**
   * Callback to chrome.fileBrowserPrivate.getFileTasks.
   * It remembers the returned task id and url. When tasks for all test cases
   * are found, they are executed.
   *
   * @param {string} fileUrl File url for which getFileTasks was called.
   * @param {Array.<Object>} tesks List of found task objects.
   */
  function onGotTasks(fileUrl, tasks) {
    if (!tasks) {
      onError('Failed getting tasks for ' + fileUrl);
      return;
    }
    if (tasks.length != 1) {
      onError('Got invalid number of tasks for "' + fileUrl + '" : ' +
              tasks.lenght);
    }

    foundTasks.push({id: tasks[0].taskId, url: fileUrl});

    if (foundTasks.length == kTestCases.length) {
      foundTasks.forEach(function(task) {
        chrome.fileBrowserPrivate.executeTask(task.id, [task.url],
            onExecuteTask.bind(null, task.url));
      });
    }
  }

  /**
   * Success callback for getFile operation. Remembers resolved test case, and
   * when all the test cases have been resolved, gets file tasks for each of
   * them.
   *
   * @param {string} mimeType The mime type for the test case.
   * @param {FileEntry} entry The file entry for the test case.
   */
  function onGotEntry(mimeType, entry) {
    resolvedTestCases.push({entry: entry, mimeType: mimeType});

    if (resolvedTestCases.length == kTestCases.length) {
      resolvedTestCases.forEach(function(testCase) {
        chrome.fileBrowserPrivate.getFileTasks(
            [testCase.entry.toURL()],
            [testCase.mimeType],
            onGotTasks.bind(null, testCase.entry.toURL()));
      });
    }
  }

  /**
   * Called when the test mount point has been determined. It starts resolving
   * test cases (i.e. getting file entries for the test file paths).
   *
   * @param {DirectoryEntry} mountPointEntry The mount points root dir entry.
   */
  function onFoundMountPoint(mountPointEntry) {
    kTestCases.forEach(function(testCase) {
      mountPointEntry.getFile(
          testCase.path, {},
          onGotEntry.bind(null, testCase.mimeType),
          onError.bind(null, 'Unable to get file: ' + testCase.path));
    });
  }

  /**
   * Callback for chrome.fileBrowserPrivate.requestLocalFileSystem.
   * When the local fileSystem is found, tries to get the test mount point root
   * dir by probing for existence of possible mount point root dirs.
   * The Chrome should have enabled either 'local/' or 'drive/' mount point.
   *
   * @param {FileSystem} fileSystem External file system.
   */
  function onGotFileSystem(fileSystem) {
    if (!fileSystem) {
      onError('Failed to get file system for test.');
      return;
    }

    var possibleMountPoints = ['local/', 'drive/'];

    function tryNextMountPoint() {
      if (possibleMountPoints.length == 0) {
        onError('Unable to find mounted mount point.');
        return;
      }

      var mountPointRoot = possibleMountPoints.shift();

      fileSystem.root.getDirectory(mountPointRoot, {},
                                   onFoundMountPoint,
                                   tryNextMountPoint);
    }

    tryNextMountPoint();
  }

  chrome.fileBrowserPrivate.requestLocalFileSystem(onGotFileSystem);
}

// Start the testing.
run();
