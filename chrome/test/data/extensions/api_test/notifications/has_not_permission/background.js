// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function hasPermission() {
    chrome.test.assertEq(1,  // permission not allowed
                         webkitNotifications.checkPermission());
    chrome.test.succeed();
  },
  function showNotification() {
    try {
      window.webkitNotifications.createHTMLNotification(
          chrome.extension.getURL("notification.html")).show();
    } catch (e) {
      chrome.test.assertTrue(e.message.indexOf("SECURITY_ERR") == 0);
      chrome.test.succeed();
      return;
    }
    chrome.test.fail("Expected access denied error.");
  }
]);
