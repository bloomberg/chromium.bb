// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
      function addRemoveDevice() {
        chrome.gcdPrivate.onDeviceRemoved.addListener(
            function(deviceId) {
              chrome.test.assertEq(deviceId,
                                   "mdns:myService._privet._tcp.local");
              chrome.test.notifyPass();
            })
  }]);
};
