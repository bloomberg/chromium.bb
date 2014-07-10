// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
      function receiveNewDevice() {
        chrome.gcdPrivate.onDeviceStateChanged.addListener(
            function(device) {
              chrome.test.assertEq(device.setupType, "mdns");
              chrome.test.assertEq(device.deviceId,
                                   "mdns:myService._privet._tcp.local");
              chrome.test.assertEq(device.deviceType, "printer");
              chrome.test.assertEq(device.deviceName,
                                   "Sample device");
              chrome.test.assertEq(device.deviceDescription,
                                   "Sample device description");

              chrome.test.notifyPass();
      })
  }]);
};
