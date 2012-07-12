// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDiscovery() {
  chrome.test.assertEq(kExpectedDeviceNames.length,
      discoveredDevices.length);
  for (var i = 0; i < kExpectedDeviceNames.length; ++i) {
    chrome.test.assertEq(kExpectedDeviceNames[i], discoveredDevices[i].name);
  }

  chrome.test.succeed();
}

function startTests() {
  chrome.test.runTests([testDiscovery]);
}

function sendReady(callback) {
  chrome.test.sendMessage('ready', callback);
}

function stopDiscoveryAndContinue() {
  chrome.experimental.bluetooth.stopDiscovery();
  sendReady(startTests);
}

var kExpectedDeviceNames = ["d1"];
var discoveredDevices = [];
function recordDevice(device) {
  discoveredDevices.push(device);
}

chrome.experimental.bluetooth.startDiscovery(
    { deviceCallback:recordDevice },
    function() { sendReady(stopDiscoveryAndContinue); });
