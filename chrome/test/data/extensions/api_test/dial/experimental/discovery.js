// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function discovery() {
      var discoverNowShouldSucceed = function(result) {
        if (result)
          chrome.test.succeed();
        else
          chrome.test.fail();
      };

      var onDeviceList = function(deviceList) {
        // Unused.
      };

      var discoverNowShouldFail = function(result) {
        if (!result) {
          chrome.test.succeed();
          chrome.dial.onDeviceList.addListener(onDeviceList);
          chrome.dial.discoverNow(discoverNowShouldSucceed);
        } else {
          chrome.test.fail();
        }
      };

      chrome.dial.discoverNow(discoverNowShouldFail);
    }
  ]);
};