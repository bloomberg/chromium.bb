// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Dummy private APIs.
 */
var chrome;

/**
 * Callbacks registered by setTimeout.
 * @type {Array.<function>}
 */
var timeoutCallbacks;

// Set up the test components.
function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    REMOVABLE_DEVICE_DETECTION_TITLE: 'Device detected',
    REMOVABLE_DEVICE_NAVIGATION_MESSAGE: 'DEVICE_NAVIGATION',
    REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL: '',
    REMOVABLE_DEVICE_IMPORT_MESSAGE: 'DEVICE_IMPORT',
    REMOVABLE_DEVICE_IMPORT_BUTTON_LABEL: '',
    DEVICE_UNKNOWN_MESSAGE: 'DEVICE_UNKNOWN: $1',
    DEVICE_UNSUPPORTED_MESSAGE: 'DEVICE_UNSUPPORTED: $1',
    DEVICE_HARD_UNPLUGGED_TITLE: 'DEVICE_HARD_UNPLUGGED_TITLE',
    DEVICE_HARD_UNPLUGGED_MESSAGE: 'DEVICE_HARD_UNPLUGGED_MESSAGE',
    MULTIPART_DEVICE_UNSUPPORTED_MESSAGE: 'MULTIPART_DEVICE_UNSUPPORTED: $1',
    EXTERNAL_STORAGE_DISABLED_MESSAGE: 'EXTERNAL_STORAGE_DISABLED',
    FORMATTING_OF_DEVICE_PENDING_TITLE: 'FORMATTING_OF_DEVICE_PENDING_TITLE',
    FORMATTING_OF_DEVICE_PENDING_MESSAGE: 'FORMATTING_OF_DEVICE_PENDING',
    FORMATTING_OF_DEVICE_FINISHED_TITLE: 'FORMATTING_OF_DEVICE_FINISHED_TITLE',
    FORMATTING_FINISHED_SUCCESS_MESSAGE: 'FORMATTING_FINISHED_SUCCESS',
    FORMATTING_OF_DEVICE_FAILED_TITLE: 'FORMATTING_OF_DEVICE_FAILED_TITLE',
    FORMATTING_FINISHED_FAILURE_MESSAGE: 'FORMATTING_FINISHED_FAILURE'
  };

  // Make dummy APIs.
  chrome = {
    commandLinePrivate: {
      hasSwitch: function(switchName, callback) {
        if (switchName === 'enable-cloud-backup') {
          callback(chrome.commandLinePrivate.cloudBackupEnabled);
        }
      },
      cloudBackupEnabled: false
    },
    fileManagerPrivate: {
      onDeviceChanged: {
        addListener: function(listener) {
          this.dispatch = listener;
        }
      },
      onMountCompleted: {
        addListener: function(listener) {
          this.dispatch = listener;
        }
      }
    },
    notifications: {
      create: function(id, params, callback) {
        this.items[id] = params;
        callback();
      },
      clear: function(id, callback) { delete this.items[id]; callback(); },
      items: {},
      onButtonClicked: {
        addListener: function(listener) {
          this.dispatch = listener;
        }
      },
      getAll: function(callback) {
        callback([]);
      }
    },
    runtime: {
      getURL: function(path) { return path; },
      onStartup: {
        addListener: function() {}
      }
    }
  };
}

function testGoodDevice() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_NAVIGATION',
      chrome.notifications.items['deviceNavigation:/device/path'].message);
}

function testMediaDeviceWithImportEnabled(testCallback) {
  // "Enable" cloud backup, then make a device handler.
  chrome.commandLinePrivate.cloudBackupEnabled = true;
  var handler = new DeviceHandler();

  importer.importEnabled()
      .then(
          function(ignored) {
            chrome.fileManagerPrivate.onMountCompleted.dispatch({
              eventType: 'mount',
              status: 'success',
              volumeMetadata: {
                isParentDevice: true,
                deviceType: 'usb',
                devicePath: '/device/path',
                deviceLabel: 'label',
                hasMedia: true
              },
              shouldNotify: true
            });
            assertEquals(1, Object.keys(chrome.notifications.items).length);
            assertTrue(
                'deviceImport:/device/path' in chrome.notifications.items,
                'Import notification not found in the notifications queue.');
            assertEquals(
                'DEVICE_IMPORT',
                chrome.notifications.items[
                    'deviceImport:/device/path'].message,
                'Import notification did not have the right message.');

            testCallback(/* was error */ false);
          })
      .catch(
          function(error) {
            console.error('TEST FAILED: ' + error);
            testCallback(/* was error */ true);
          });
}

function testMediaDeviceWithImportDisabled(testCallback) {
  // "Disable" cloud backup, then make a device handler.
  chrome.commandLinePrivate.cloudBackupEnabled = false;
  var handler = new DeviceHandler();

  importer.importEnabled()
      .then(
          function(ignored) {
            chrome.fileManagerPrivate.onMountCompleted.dispatch({
              eventType: 'mount',
              status: 'success',
              volumeMetadata: {
                isParentDevice: true,
                deviceType: 'usb',
                devicePath: '/device/path',
                deviceLabel: 'label',
                hasMedia: true
              },
              shouldNotify: true
            });
            assertEquals(1, Object.keys(chrome.notifications.items).length);
            assertFalse(
                'deviceImport:/device/path' in chrome.notifications.items,
                'Unexpected import notification found in notifications queue.');
            assertEquals(
                'DEVICE_NAVIGATION',
                chrome.notifications.items[
                    'deviceNavigation:/device/path'].message,
                'Device notification did not have the right message.');

            testCallback(/* was error */ false);
          })
      .catch(
          function(error) {
            console.error('TEST FAILED: ' + error);
            testCallback(/* was error */ true);
          });
}

function testGoodDeviceNotNavigated() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: false
  });
  assertEquals(0, Object.keys(chrome.notifications.items).length);
}

function testGoodDeviceWithBadParent() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertFalse(!!chrome.notifications.items['device:/device/path']);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      chrome.notifications.items['deviceFail:/device/path'].message);

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_NAVIGATION',
      chrome.notifications.items['deviceNavigation:/device/path'].message);

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  // Should do nothing this time.
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_NAVIGATION',
      chrome.notifications.items['deviceNavigation:/device/path'].message);
}

function testUnsupportedDevice() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertFalse(!!chrome.notifications.items['device:/device/path']);
  assertEquals(
      'DEVICE_UNSUPPORTED: label',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testUnsupportedWithUnknownParent() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(
      'DEVICE_UNKNOWN: label',
      chrome.notifications.items['deviceFail:/device/path'].message);

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNSUPPORTED: label',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testMountPartialSuccess() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_NAVIGATION',
      chrome.notifications.items['deviceNavigation:/device/path'].message);

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unsupported_filesystem',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(2, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'MULTIPART_DEVICE_UNSUPPORTED: label',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testUnknown() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_unknown',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testNonASCIILabel() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      // "RA (U+30E9) BE (U+30D9) RU (U+30EB)" in Katakana letters.
      deviceLabel: '\u30E9\u30D9\u30EB'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: \u30E9\u30D9\u30EB',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testMulitpleFail() {
  // Make a device handler.
  var handler = new DeviceHandler();

  // The first parent error.
  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: true,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      chrome.notifications.items['deviceFail:/device/path'].message);

  // The first child error that replaces the parent error.
  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'DEVICE_UNKNOWN: label',
      chrome.notifications.items['deviceFail:/device/path'].message);

  // The second child error that turns to a multi-partition error.
  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'MULTIPART_DEVICE_UNSUPPORTED: label',
      chrome.notifications.items['deviceFail:/device/path'].message);

  // The third child error that should be ignored because the error message does
  // not changed.
  chrome.fileManagerPrivate.onMountCompleted.dispatch({
    eventType: 'mount',
    status: 'error_internal',
    volumeMetadata: {
      isParentDevice: false,
      deviceType: 'usb',
      devicePath: '/device/path',
      deviceLabel: 'label'
    },
    shouldNotify: true
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals(
      'MULTIPART_DEVICE_UNSUPPORTED: label',
      chrome.notifications.items['deviceFail:/device/path'].message);
}

function testDisabledDevice() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'disabled',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('EXTERNAL_STORAGE_DISABLED',
               chrome.notifications.items['deviceFail:/device/path'].message);

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'removed',
    devicePath: '/device/path'
  });
  assertEquals(0, Object.keys(chrome.notifications.items).length);
}

function testFormatSucceeded() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_start',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('FORMATTING_OF_DEVICE_PENDING',
               chrome.notifications.items['formatStart:/device/path'].message);

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_success',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('FORMATTING_FINISHED_SUCCESS',
               chrome.notifications.items[
                   'formatSuccess:/device/path'].message);
}

function testFormatFailed() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_start',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('FORMATTING_OF_DEVICE_PENDING',
               chrome.notifications.items['formatStart:/device/path'].message);

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'format_fail',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('FORMATTING_FINISHED_FAILURE',
               chrome.notifications.items['formatFail:/device/path'].message);
}

function testDeviceHardUnplugged() {
  // Make a device handler.
  var handler = new DeviceHandler();

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'hard_unplugged',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('DEVICE_HARD_UNPLUGGED_MESSAGE',
               chrome.notifications.items[
                   'hardUnplugged:/device/path'].message);
}
