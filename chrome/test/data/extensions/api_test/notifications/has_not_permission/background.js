// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function hasPermission() {
    chrome.test.assertEq(1,  // permission not allowed
                         webkitNotifications.checkPermission());
    chrome.test.succeed();
  },
  function showHTMLNotification() {
    // createHTMLNotification should not be exposed.
    if (window.webkitNotifications.createHTMLNotification)
      chrome.test.fail("createHTMLNotification is found.");
    else
      chrome.test.succeed();
  },
  function showTextNotification() {
    try {
      window.webkitNotifications.createNotification(
          "", "Foo", "This is text notification.").show();
    } catch (e) {
      chrome.test.assertTrue(e.message.indexOf("SecurityError") == 0);
      chrome.test.succeed();
      return;
    }
    chrome.test.fail("Expected access denied error.");
  }
]);
