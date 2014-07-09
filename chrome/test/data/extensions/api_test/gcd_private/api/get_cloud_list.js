// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function getCloudList() {
      chrome.gcdPrivate.getCloudDeviceList(function(services) {
        // Sort to avoid order dependence
        services.sort(function(a,b) {
          return a.deviceId.localeCompare(b.deviceId);
        });

        chrome.test.assertEq(2, services.length);

        chrome.test.assertEq(services[0].setupType, "cloud");
        chrome.test.assertEq(services[0].deviceId,
                             "cloudprint:someCloudPrintID");
        chrome.test.assertEq(services[0].cloudId, "someCloudPrintID");
        chrome.test.assertEq(services[0].deviceType, "printer");
        chrome.test.assertEq(services[0].deviceName,
                             "someCloudPrintDisplayName");
        chrome.test.assertEq(services[0].deviceDescription,
                             "someCloudPrintDescription");

        chrome.test.assertEq(services[1].setupType, "cloud");
        chrome.test.assertEq(services[1].deviceId, "gcd:someGCDID");
        chrome.test.assertEq(services[1].cloudId, "someGCDID");
        chrome.test.assertEq(services[1].deviceType, "someType");
        chrome.test.assertEq(services[1].deviceName, "someGCDDisplayName");
        chrome.test.assertEq(services[1].deviceDescription,
                             "someGCDDescription");

        chrome.test.notifyPass();
    })
  }]);
};
