// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// accessibility.getAlertsForTab test for Chrome
// browser_tests --gtest_filter="ExtensionApiTest.GetAlertsForTab"

chrome.test.runTests([
  function oneAlert() {
    chrome.tabs.query({'active': true}, function(tabs) {
      chrome.test.assertEq(tabs.length, 1);
      chrome.accessibilityPrivate.getAlertsForTab(
          tabs[0].id, function(alerts) {
        chrome.test.assertEq(alerts.length, 1);
        chrome.test.assertEq(alerts[0].message, 'Simple Alert Infobar.');
        chrome.test.succeed();
      });
    });
  },
  function noAlert() {
    chrome.tabs.create({});
    chrome.tabs.query({'active': true}, function(tabs) {
      chrome.test.assertEq(tabs.length, 1);
      chrome.accessibilityPrivate.getAlertsForTab(
          tabs[0].id, function(alerts) {
        chrome.test.assertEq(alerts.length, 0);
        chrome.test.succeed();
      });
    });
  }
]);
