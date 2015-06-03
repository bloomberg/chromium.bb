// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabId = -1;

chrome.pageAction.onClicked.addListener(function(tab) {
  chrome.pageAction.hide(tabId);
  chrome.test.sendMessage('clicked');
});

chrome.tabs.getSelected(null, function(tab) {
  tabId = tab.id;
  chrome.pageAction.show(tabId);
  chrome.test.sendMessage('ready');
});
