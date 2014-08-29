// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function discoverySucceeds() {
      var discoverNowCallback = function(result) {
        chrome.test.assertNoLastError();
        // The result may be true or false depending on whether periodic
        // discovery is running at the same time.
        // TODO(mfoltz): We may want to update the API to distinguish error cases
        // from simultaneous discovery.
        chrome.test.assertTrue(typeof(result) == 'boolean');
        chrome.test.succeed();
      };
      chrome.dial.onDeviceList.addListener(function(deviceList) {});
      chrome.dial.discoverNow(discoverNowCallback);
    }
  ]);
};