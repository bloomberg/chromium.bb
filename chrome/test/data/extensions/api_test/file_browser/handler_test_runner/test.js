// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs test to verify that file browser tasks can successfully be executed.
 * The test does the following:
 * - Open external filesystem.
 * - Get the test file system. The root is determined by probing volumes with
 *   whitelisted ids.
 * - Get files 'test_dir/test_file.xul' and 'test_dir/test_file.tiff'
 *   on the test mount point.
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
 * @type {Array.<string>}
 */
var kTestPaths = ['test_dir/test_file.xul', 'test_dir/test_file.tiff'];

// Starts the test extension.
function run() {
  /**
   * Test cases after the file path has been resolved to FileEntry. Each
   * resolved test case contains the resolved FileEntry object.
   *
   * @type Array.<FileEntry>
   */
  var resolvedEntries = [];

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
   * It checks that the returned task is not the default, sets it as the default
   * and calls getFileTasks again.
   *
   * @param {string} fileUrl File url for which getFileTasks was called.
   * @param {Array.<Object>} tasks List of found task objects.
   */

  function onGotNonDefaultTasks(fileUrl, tasks) {
    if (!tasks) {
      onError('Failed getting tasks for ' + fileUrl);
      return;
    }
    if (tasks.length != 1) {
      onError('Got invalid number of tasks for "' + fileUrl + '": ' +
              tasks.length);
    }
    if (tasks[0].isDefault) {
      onError('Task "' + tasks[0].taskId + '" is default for "' + fileUrl +
          '"');
    }
    chrome.fileBrowserPrivate.setDefaultTask(
        tasks[0].taskId, [fileUrl],
        chrome.fileBrowserPrivate.getFileTasks.bind(null, [fileUrl],
            onGotTasks.bind(null, fileUrl)));
  }

  /**
   * Callback to chrome.fileBrowserPrivate.getFileTasks.
   * It remembers the returned task id and url. When tasks for all test cases
   * are found, they are executed.
   *
   * @param {string} fileUrl File url for which getFileTasks was called.
   * @param {Array.<Object>} tasks List of found task objects.
   */
  function onGotTasks(fileUrl, tasks) {
    if (!tasks) {
      onError('Failed getting tasks for ' + fileUrl);
      return;
    }
    if (tasks.length != 1) {
      onError('Got invalid number of tasks for "' + fileUrl + '": ' +
              tasks.length);
    }
    if (!tasks[0].isDefault) {
      onError('Task "' + tasks[0].taskId + '" is not default for "' + fileUrl +
          '"');
    }

    foundTasks.push({id: tasks[0].taskId, url: fileUrl});

    if (foundTasks.length == kTestPaths.length) {
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
   * @param {FileEntry} entry The file entry for the test case.
   */
  function onGotEntry(entry) {
    resolvedEntries.push(entry);

    if (resolvedEntries.length == kTestPaths.length) {
      resolvedEntries.forEach(function(entry) {
        chrome.fileBrowserPrivate.getFileTasks(
            [entry.toURL()],
            onGotNonDefaultTasks.bind(null, entry.toURL()));
      });
    }
  }

  /**
   * Called when the test mount point has been determined. It starts resolving
   * test cases (i.e. getting file entries for the test file paths).
   *
   * @param {DOMFileSystem} fileSystem The testing volume.
   * @param {string} volumeType Type of the volume.
   */
  function onGotFileSystem(fileSystem, volumeType) {
    var isOnDrive = volumeType == 'drive';
    kTestPaths.forEach(function(filePath) {
      fileSystem.root.getFile(
          (isOnDrive ? 'root/' : '') + filePath, {},
          onGotEntry.bind(null),
          onError.bind(null, 'Unable to get file: ' + filePath));
    });
  }

  chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    // Try to acquire the first volume which is either TESTING or DRIVE type.
    var possibleVolumeTypes = ['testing', 'drive'];
    var sortedVolumeMetadataList = volumeMetadataList.filter(function(volume) {
      return possibleVolumeTypes.indexOf(volume.volumeType) != -1;
    }).sort(function(volumeA, volumeB) {
      return possibleVolumeTypes.indexOf(volumeA.volumeType) >
             possibleVolumeTypes.indexOf(volumeB.volumeType);
    });
    if (sortedVolumeMetadataList.length == 0) {
      onError('No volumes available, which could be used for testing.');
      return;
    }
    chrome.fileBrowserPrivate.requestFileSystem(
        sortedVolumeMetadataList[0].volumeId,
        function(fileSystem) {
          if (!fileSystem) {
            onError('Failed to acquire the testing volume.');
            return;
          }
          onGotFileSystem(fileSystem, sortedVolumeMetadataList[0].volumeType);
        });
  });
}

// Start the testing.
run();
