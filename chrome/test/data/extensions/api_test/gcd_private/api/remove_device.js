// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function addRemoveDevice() {
      var should_be_available = true;
      chrome.gcdPrivate.onCloudDeviceStateChanged.addListener(
        function(available, device) {
          chrome.test.assertEq(available, should_be_available);
          should_be_available = false;

          chrome.test.assertEq(device.setupType, "mdns");
          chrome.test.assertEq(device.idString,
                               "mdns:myService._privet._tcp.local");
          chrome.test.assertEq(device.deviceType, "printer");
          chrome.test.assertEq(device.deviceName,
                               "Sample device");
          chrome.test.assertEq(device.deviceDescription,
                               "Sample device description");

          if (!available) {
            // Only pass after device is removed
            chrome.test.notifyPass();
          }
    })
  }]);
};
