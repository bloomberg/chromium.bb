// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Test target.
 * @type {DeviceHandler}
 */
var handler;

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
    REMOVABLE_DEVICE_SCANNING_MESSAGE: 'Scanning...',
    REMOVABLE_DEVICE_NAVIGATION_MESSAGE: 'DEVICE_NAVIGATION',
    REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL: '',
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
      }
    },
    runtime: {
      getURL: function(path) { return path; },
      onStartup: {
        addListener: function() {}
      }
    }
  };

  // Make a device handler.
  handler = new DeviceHandler();
}

function testGoodDevice() {
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

function testGoodDeviceNotNavigated() {
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

function testScanCanceled() {
  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'scan_started',
    devicePath: '/device/path'
  });
  assertTrue('device:/device/path' in chrome.notifications.items);
  assertEquals('Scanning...',
               chrome.notifications.items['device:/device/path'].message);

  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'scan_cancelled',
    devicePath: '/device/path'
  });
  assertEquals(0, Object.keys(chrome.notifications.items).length);

  // Nothing happened.
  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'removed',
    devicePath: '/device/path'
  });
  assertEquals(0, Object.keys(chrome.notifications.items).length);
}

function testDisabledDevice() {
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
  chrome.fileManagerPrivate.onDeviceChanged.dispatch({
    type: 'hard_unplugged',
    devicePath: '/device/path'
  });
  assertEquals(1, Object.keys(chrome.notifications.items).length);
  assertEquals('DEVICE_HARD_UNPLUGGED_MESSAGE',
               chrome.notifications.items[
                   'hardUnplugged:/device/path'].message);
}
