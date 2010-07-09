// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Checks that there is only one window and one tab, and calls back |callback|
// with its id (or -1 if there is more than 1 window or more than 1 tab).
function getCurrentSingleTabId(callback) {
  chrome.windows.getAll({"populate":true}, function(windows) {
    if (windows.length != 1 || windows[0].tabs.length != 1) {
      callback(-1);
    } else {
      callback(windows[0].tabs[0].id);
    }
  });
}

function navigateCurrentTab(url) {
  getCurrentSingleTabId(function(tabid) {
    chrome.tabs.update(tabid, {"url": url});
  });
}

function onclick(info) {
  navigateCurrentTab(chrome.extension.getURL("test2.html"));
}

window.onload = function() {
  chrome.experimental.contextMenus.create({"title":"Extension Item 1",
                                          "onclick": onclick}, function(id) {
    if (!chrome.extension.lastError) {
      navigateCurrentTab(chrome.extension.getURL("test.html"));
    }
  });
};
