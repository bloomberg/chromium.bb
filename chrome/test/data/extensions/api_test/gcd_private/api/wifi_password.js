// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function wifiPassword() {
      var succeeded = false;
      var failed = false;

      function expectResult(expectedResult, result) {
        chrome.test.assertEq(expectedResult, result);

        if (expectedResult) succeeded = true;
        else failed = true;

        if (succeeded && failed) {
          chrome.gcdPrivate.getPrefetchedWifiNameList(onWifiList);
        }
      }

      function onWifiList(list) {
        chrome.test.assertEq(["SuccessNetwork"], list);

        chrome.test.notifyPass();
      }

      chrome.gcdPrivate.prefetchWifiPassword("SuccessNetwork",
                                             expectResult.bind(null, true));

      chrome.gcdPrivate.prefetchWifiPassword("FailureNetwork",
                                             expectResult.bind(null, false));
    }
  ]);
};
