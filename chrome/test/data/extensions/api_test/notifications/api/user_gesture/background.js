// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function userActionTest() {
  chrome.test.sendMessage("");
}

chrome.notifications.onClicked.addListener(userActionTest);
chrome.notifications.onButtonClicked.addListener(userActionTest);
chrome.notifications.onClosed.addListener(userActionTest);

chrome.notifications.create("test", {
  iconUrl: "",
  title: "hi",
  message: "there",
  type: "basic",
  buttons: [{title: "Button"}, {title: "Button"}],
}, function () {});
