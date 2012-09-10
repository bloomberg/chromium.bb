// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enable the page action on this tab.
chrome.tabs.getSelected(null, function(tab) {
  chrome.pageAction.setIcon({tabId: tab.id, iconIndex: 1}, function(){
    chrome.test.notifyPass();
  });
});
