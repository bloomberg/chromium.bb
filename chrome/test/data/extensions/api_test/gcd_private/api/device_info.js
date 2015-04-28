// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function deviceInfo() {
      function onDeviceInfo(status, info) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq(info["version"], "3.0");
        window.setTimeout(chrome.test.notifyPass,3000);
      }

      chrome.gcdPrivate.getDeviceInfo("myService._privet._tcp.local",
                                      onDeviceInfo);
    }
  ]);
};
