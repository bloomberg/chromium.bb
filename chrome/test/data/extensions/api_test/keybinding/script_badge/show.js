// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Show the script badge now.
chrome.tabs.getSelected(null, function(tab) {
  chrome.scriptBadge.getAttention({ "tabId": tab.id });
  chrome.test.notifyPass();
});
