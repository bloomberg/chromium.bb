// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Page Action ID.
var pageActionId = "TestId";

// The window this Page Action is associated with.
var windowId = -1;

// The TabId this Page Action is associated with.
var tabId = -1;

// The URL of the page on build.chromium.org.
var pageUrl = "";

chrome.self.onConnect.addListener(function(port) {
  windowId = port.tab.windowId;
  tabId = port.tab.id;
  pageUrl = port.tab.url;

  port.onMessage.addListener(function(mybool) {
    // Let Chrome know that the PageAction needs to be enabled for this tabId
    // and for the url of this page.
    chrome.pageActions.enableForTab(pageActionId,
                                    {tabId: tabId, url: pageUrl});
  });
});
