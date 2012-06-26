// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that we can change the popup for the script badge.
// The C++ verifies.
chrome.tabs.getSelected(null, function(tab) {
  chrome.scriptBadge.setPopup({tabId: tab.id, popup: "set_popup.html"});
  chrome.test.notifyPass();
});
