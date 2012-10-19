// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevicesConcurrently() {
  chrome.test.succeed();
}

function nop() {}

try {
  var getDevicesOptions = { deviceCallback:nop, name:'foo' };
  chrome.bluetooth.getDevices(getDevicesOptions, nop);
  chrome.bluetooth.getDevices(getDevicesOptions, nop);
} catch(e) {
  chrome.test.sendMessage('ready',
      function(message) {
        chrome.test.runTests([testGetDevicesConcurrently]);
      });
}
