// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.callbackFail;
var pass = chrome.test.callbackPass;

var PERMISSION_DENIED_ERROR = 'Permission to access device denied';

var kDevice = {
  address: '11:12:13:14:15:16',
  name: 'Test Device',
  paired: true,
  connected: false
};

var kWrongDevice = {
  address: '55:44:33:22:11:00',
  name: 'Wrong Device',
  paired: true,
  connected: false
};

var kProfile = {uuid: '00001101-0000-1000-8000-00805f9b34fb'};

chrome.test.runTests([
  function permissionRequest() {
    chrome.permissions.request(
      {
        permissions: [
          {'bluetoothDevices': [{'deviceAddress': kDevice.address}]}
        ]
      },
      pass(function(granted) {
        // They were not granted, and there should be no error.
        assertTrue(granted);
        assertTrue(chrome.runtime.lastError === undefined);

        chrome.bluetooth.connect(
          {device: kDevice, profile: kProfile},
          pass(function() {}));
      }));
  },
  function permissionDenied() {
    chrome.bluetooth.connect(
      {device: kWrongDevice, profile: kProfile},
      fail(PERMISSION_DENIED_ERROR));
  }
]);
