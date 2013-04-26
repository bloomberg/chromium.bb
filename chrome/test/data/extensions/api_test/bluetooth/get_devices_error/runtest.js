// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevicesReturnsError() {
  chrome.test.assertEq("Invalid UUID", error);
  chrome.test.succeed();
}

var error = "";
chrome.bluetooth.getDevices(
  {
    profile: {uuid:'this is nonsense'},
    deviceCallback: function() {}
  },
  function() {
    if (chrome.runtime.lastError) {
      error = chrome.runtime.lastError.message;
    }
    chrome.test.sendMessage('ready',
      function(message) {
        chrome.test.runTests([testGetDevicesReturnsError]);
      });
  });
