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

var make_browsertest_proceed = function() {
  if (!chrome.extension.lastError) {
    navigateCurrentTab(chrome.extension.getURL("test.html"));
  }
};

var patterns = ["http://*.google.com/*", "https://*.google.com/*"];

window.onload = function() {
  // Create one item that does have a documentUrlPattern and targetUrlPattern.
  var properties1 = {
    "title": "test_item1", "documentUrlPatterns": patterns,
    "targetUrlPatterns": patterns
  };
  chrome.contextMenus.create(properties1);

  // Create an item that initially doesn't have a documentUrlPattern and
  // targetUrlPattern, then update it, and trigger the rest of the c++ code in
  // the browser test by navigating the tab.
  var properties2 = { "title": "test_item2" };

  var id2;
  id2 = chrome.contextMenus.create(properties2,
                                                function() {
    var update_properties = { "documentUrlPatterns": patterns,
                              "targetUrlPatterns": patterns };
    chrome.contextMenus.update(id2, update_properties,
                               make_browsertest_proceed);
  });
};
