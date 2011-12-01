// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  // TODO: look for 'stegosaurus' in the page contents.
  if (tab.url.search("stegosaurus") > -1)
    chrome.pageAction.show(tabId);
  else
    chrome.pageAction.hide(tabId);
});
