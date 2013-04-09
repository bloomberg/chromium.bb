// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function HTMLNotificationNotFound() {
    // createHTMLNotification should not be exposed.
    if (window.webkitNotifications.createHTMLNotification)
      chrome.test.fail("createHTMLNotification is found.");
    else
      chrome.test.succeed();
  },
  function TextNotificationFound() {
    if (!window.webkitNotifications.createNotification)
      chrome.test.fail("createNotification not found.");
    else
      chrome.test.succeed();
  }
]);

