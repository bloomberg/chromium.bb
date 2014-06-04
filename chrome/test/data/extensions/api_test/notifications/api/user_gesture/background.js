// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const red_dot = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA" +
    "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO" +
    "9TXL0Y4OHwAAAABJRU5ErkJggg==";

function userActionTest() {
  chrome.test.sendMessage("");
}

chrome.notifications.onClicked.addListener(userActionTest);
chrome.notifications.onButtonClicked.addListener(userActionTest);
chrome.notifications.onClosed.addListener(userActionTest);

chrome.notifications.create("test", {
  iconUrl: red_dot,
  title: "hi",
  message: "there",
  type: "basic",
  buttons: [{title: "Button"}, {title: "Button"}],
}, function () {});
