// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var notification = null;

// Shows the notification window using the specified URL.
// Control continues at onNotificationDone().
function showNotification(url) {
  notification = window.webkitNotifications.createHTMLNotification(url);
  notification.onerror = function() {
    chrome.test.fail("Failed to show notification.");
  };
  notification.show();
}

// Called by the notification when it is done with its tests.
function onNotificationDone(notificationWindow) {
  var views = chrome.extension.getViews();
  chrome.test.assertEq(2, views.length);
  notificationWindow.onunload = function() {
    chrome.test.succeed();
  }
  notification.cancel();
}

chrome.test.runTests([
  function hasPermission() {
    chrome.test.assertEq(0,  // allowed
                         webkitNotifications.checkPermission());
    chrome.test.succeed();
  },
  function absoluteURL() {
    showNotification(chrome.extension.getURL("notification.html"));
  },
  function relativeURL() {
    showNotification("notification.html");
  }
]);
