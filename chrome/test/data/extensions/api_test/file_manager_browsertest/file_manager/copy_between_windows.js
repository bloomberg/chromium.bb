// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Opens two window of given root paths.
 * @param {string} rootPath1 Root path of the first window.
 * @param {string} rootPath2 Root path of the second window.
 * @return {Promise} Promise fulfilled with an array containing two window IDs.
 */
function openTwoWindows(rootPath1, rootPath2) {
  return Promise.all([
    openNewWindow(null, rootPath1),
    openNewWindow(null, rootPath2)
  ]).then(function(windowIds) {
    return Promise.all([
      waitForElement(windowIds[0], '#detail-table'),
      waitForElement(windowIds[1], '#detail-table'),
    ]).then(function() {
      return windowIds;
    });
  });
};

/**
 * Copies a file between two windows.
 * @param {string} windowId1 ID of the source window.
 * @param {string} windowId2 ID of the destination window.
 * @param {TestEntryInfo} file Test entry info to be copied.
 * @return {Promise} Promise fulfilled on success.
 */
function copyBetweenWindows(windowId1, windowId2, file) {
  // Select the file.
  return waitForFiles(windowId1, [file.getExpectedRow()]).
  then(function() {
    return callRemoteTestUtil('selectFile', windowId1, [file.nameText]);
  }).
  then(function(result) {
    chrome.test.assertTrue(result);
    callRemoteTestUtil('execCommand', windowId1, ['copy']);
  }).
  then(function() {
    return waitForFiles(windowId2, []);
  }).
  then(function() {
    // Paste it.
    return callRemoteTestUtil('execCommand', windowId2, ['paste']);
  }).
  then(function() {
    return waitForFiles(windowId2,
                        [file.getExpectedRow()],
                        {ignoreLastModifiedTime: true});
  });
};

var REMOVABLE_VOLUME_QUERY = '#directory-tree > .tree-item > .tree-row ' +
    '.volume-icon[volume-type-icon="removable"]';

testcase.copyBetweenWindowsDriveToLocal = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
      // Make a file in a drive directory.
      function() {
        addEntries(['drive'], [ENTRIES.hello], this.next);
      },
      // Open windows.
      function(result) {
        chrome.test.assertTrue(result);
        openTwoWindows(RootPath.DRIVE, RootPath.DOWNLOADS).then(this.next);
      },
      // Wait for a file in the Drive directory.
      function(appIds) {
        windowId1 = appIds[0];
        windowId2 = appIds[1];
        waitForFiles(windowId1,
                     [ENTRIES.hello.getExpectedRow()]).then(this.next);
      },
      // Copy a file between windows.
      function() {
        copyBetweenWindows(windowId1,
                           windowId2,
                           ENTRIES.hello).then(this.next);
      },
      function() {
        checkIfNoErrorsOccured(this.next);
      }
  ]);
};

testcase.copyBetweenWindowsDriveToUsb = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
      // Make a file in a drive directory.
      function() {
        addEntries(['drive'], [ENTRIES.hello], this.next);
      },
      // Open windows.
      function(result) {
        chrome.test.assertTrue(result);
        openTwoWindows(RootPath.DRIVE, RootPath.DRIVE).then(this.next);
      },
      // Wait for a file in the Drive directory.
      function(appIds) {
        windowId1 = appIds[0];
        windowId2 = appIds[1];
        waitForFiles(windowId1,
                     [ENTRIES.hello.getExpectedRow()]).then(this.next);
      },
      // Mount a fake USB volume.
      function() {
        chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsb'}),
                                this.next);
      },
      // Wait for the mount.
      function(result) {
        waitForElement(windowId2, REMOVABLE_VOLUME_QUERY).then(this.next);
      },
      // Click the USB volume.
      function() {
        callRemoteTestUtil(
            'fakeMouseClick', windowId2, [REMOVABLE_VOLUME_QUERY], this.next);
      },
      // Copy a file between windows.
      function(appIds) {
        copyBetweenWindows(windowId1, windowId2, ENTRIES.hello).then(this.next);
      },
      function() {
        checkIfNoErrorsOccured(this.next);
      }
    ]);
};

testcase.copyBetweenWindowsLocalToDrive = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Make a file in a local directory.
    function() {
      addEntries(['local'], [ENTRIES.hello], this.next);
    },
    // Open windows.
    function(result) {
      chrome.test.assertTrue(result);
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Copy a file between windows.
    function(appIds) {
      copyBetweenWindows(appIds[0], appIds[1], ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

testcase.copyBetweenWindowsLocalToUsb = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Make a file in a local directory.
    function() {
      addEntries(['local'], [ENTRIES.hello], this.next);
    },
    // Open windows.
    function(result) {
      chrome.test.assertTrue(result);
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DOWNLOADS).then(this.next);
    },
    // Wait for a file in the Downloads directory.
    function(appIds) {
      windowId1 = appIds[0];
      windowId2 = appIds[1];
      waitForFiles(windowId2, [ENTRIES.hello.getExpectedRow()]).then(this.next);
    },
    // Mount a fake USB volume.
    function() {
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsb'}),
                              this.next);
    },
    // Wait for the mount.
    function(result) {
      waitForElement(windowId2, REMOVABLE_VOLUME_QUERY).then(this.next);
    },
    // Click the USB volume.
    function() {
      callRemoteTestUtil(
          'fakeMouseClick', windowId2, [REMOVABLE_VOLUME_QUERY], this.next);
    },
    // Copy a file between windows.
    function() {
      copyBetweenWindows(windowId1, windowId2, ENTRIES.hello).then(this.next);
    }
  ]);
};

testcase.copyBetweenWindowsUsbToDrive = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Open windows.
    function() {
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Mount a fake USB.
    function(appIds) {
      windowId1 = appIds[0];
      windowId2 = appIds[1];
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsb'}),
                              this.next);
    },
    // Add a file to USB.
    function(result) {
      chrome.test.assertTrue(JSON.parse(result));
      addEntries(['usb'], [ENTRIES.hello], this.next);
    },
    // Wait for the mount.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(windowId1, REMOVABLE_VOLUME_QUERY).then(this.next);
    },
    // Click the volume.
    function() {
      callRemoteTestUtil(
          'fakeMouseClick', windowId1, [REMOVABLE_VOLUME_QUERY], this.next);
    },
    // Copy a file between windows.
    function() {
      copyBetweenWindows(windowId1, windowId2, ENTRIES.hello).then(this.next);
    }
  ]);
};

testcase.copyBetweenWindowsUsbToLocal = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Open windows.
    function() {
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Mount a fake USB.
    function(appIds) {
      windowId1 = appIds[0];
      windowId2 = appIds[1];
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsb'}),
                              this.next);
    },
    // Add a file to USB.
    function(result) {
      chrome.test.assertTrue(JSON.parse(result));
      addEntries(['usb'], [ENTRIES.hello], this.next);
    },
    // Wait for the mount.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(windowId1, REMOVABLE_VOLUME_QUERY).then(this.next);
    },
    // Click the volume.
    function() {
      callRemoteTestUtil(
          'fakeMouseClick', windowId1, [REMOVABLE_VOLUME_QUERY], this.next);
    },
    // Copy a file between windows.
    function(appIds) {
      copyBetweenWindows(windowId1, windowId2, ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

