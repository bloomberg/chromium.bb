// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function hasPermission() {
    chrome.test.assertEq(0,  // allowed
                         webkitNotifications.checkPermission());
    chrome.test.succeed();
  },
  function showHTMLNotification() {
    // createHTMLNotification is not exposed even when the web page permission
    // is granted.
    if (window.webkitNotifications.createHTMLNotification)
      chrome.test.fail("createHTMLNotification is found.");
    else
      chrome.test.succeed();
  },
  function showTextNotification() {
    var notification = window.webkitNotifications.createNotification(
        "", "Foo", "This is text notification.");
    notification.onerror = function() {
      chrome.test.fail("Failed to show notification.");
    };
    notification.ondisplay = function() {
      chrome.test.succeed();
    };
    notification.show();
  }
]);
