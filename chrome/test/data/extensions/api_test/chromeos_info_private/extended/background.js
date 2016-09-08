// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.getConfig(function(config) {
    var testName = config.customArg;
    if (!testName) {
      chrome.test.fail("Missing test name.");
      return;
    }
    chrome.chromeosInfoPrivate.get(['sessionType', 'playStoreStatus'],
        function (values) {
      if (testName == 'kiosk') {
        chrome.test.assertEq('kiosk', values['sessionType']);
      } else if (testName == 'arc available') {
        chrome.test.assertEq('available', values['playStoreStatus']);
      } else if (testName == 'arc enabled') {
        chrome.test.assertEq('enabled', values['playStoreStatus']);
      }
      chrome.test.succeed();
    });
  });
});
