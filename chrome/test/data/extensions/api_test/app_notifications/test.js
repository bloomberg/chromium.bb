// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = "This API is not accessible by 'split' mode extensions" +
    " in incognito windows.";

chrome.test.runTests([
  function notify() {
    chrome.experimental.app.notify(
        {}, chrome.test.callbackFail(error));
  },

  function clearAllNotifications() {
    chrome.experimental.app.clearAllNotifications(
        {}, chrome.test.callbackFail(error));
  },

  function getChannel() {
    chrome.appNotifications.getChannel(
        {clientId: "dummy_id"},
        chrome.test.callbackPass(function(channelId, e) {
      chrome.test.assertEq(error, e);
    }));
  }
]);
