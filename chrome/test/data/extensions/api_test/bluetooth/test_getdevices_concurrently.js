// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevicesConcurrently() {
  chrome.test.assertEq(1, errors.length);

  var kExpectedError = "Concurrent calls to getDevices are not allowed.";
  chrome.test.assertEq(kExpectedError, errors[0]);

  chrome.test.succeed();
}

function nop() {}

var errors = [];
function recordError() {
  if (chrome.runtime.lastError) {
    errors.push(chrome.runtime.lastError.message);
  }
}

var getDevicesOptions = { deviceCallback:nop, name:'foo' };
chrome.experimental.bluetooth.getDevices(getDevicesOptions, recordError);
chrome.experimental.bluetooth.getDevices(getDevicesOptions, recordError);
chrome.test.sendMessage('ready',
    function(message) {
      chrome.test.runTests([
          testGetDevicesConcurrently
      ]);
  });
