// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var min = 1;
var max = 5;
var current = min;

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  current++;
  if (current > max)
    current = min;

  chrome.browserAction.setIcon({
    path: "icon" + current + ".png",
    tabId: tab.id
  });
  chrome.browserAction.setTitle({
    title: "Showing icon " + current,
    tabId: tab.id
  });
  chrome.browserAction.setBadgeText({
    text: String(current),
    tabId: tab.id
  });
  chrome.browserAction.setBadgeBackgroundColor({
    color: [50*current,0,0,255],
    tabId: tab.id
  });

  chrome.test.notifyPass();
});

chrome.test.notifyPass();
