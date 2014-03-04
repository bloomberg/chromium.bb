// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests executing the default task when there is only one task.
 * @param {boolean} drive Whether to test Drive or Downloads.
 */
function executeDefaultTask(drive) {
  var path = drive ? RootPath.DRIVE : RootPath.DOWNLOADS;
  var taskId = drive ? 'dummytaskid|drive|open-with' : 'dummytaskid|open-with';
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Override tasks list with a dummy task.
    function(inAppId, inFileListBefore) {
      appId = inAppId;

      callRemoteTestUtil(
          'overrideTasks',
          appId,
          [[
            {
              driveApp: false,
              iconUrl: 'chrome://theme/IDR_DEFAULT_FAVICON',  // Dummy icon
              isDefault: true,
              taskId: taskId,
              title: 'The dummy task for test'
            }
          ]],
          this.next);
    },
    // Select file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'selectFile', appId, ['hello.txt'], this.next);
    },
    // Double-click the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
    },
    // Wait until the task is executed.
    function(result) {
      chrome.test.assertTrue(!!result);
      waitUntilTaskExecutes(appId, taskId).then(this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.executeDefaultTaskOnDrive = executeDefaultTask.bind(null, true);
testcase.executeDefaultTaskOnDownloads = executeDefaultTask.bind(null, false);
