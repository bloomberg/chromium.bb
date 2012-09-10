// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstTabId;
var firstWindowId;

chrome.test.runTests([
  function setupWindow() {
    createWindow([pageUrl("a")], {},
                 pass(function(winId, tabIds) {
      firstWindowId = winId;
      firstTabId = tabIds[0];
    }));
  },

  function duplicateTab() {
    chrome.tabs.duplicate(firstTabId, pass(function(tab) {
      assertEq(pageUrl("a"), tab.url);
      assertEq(1, tab.index);
    }));
  },

  function totalTab() {
    chrome.tabs.getAllInWindow(firstWindowId,
      pass(function(tabs) {
        assertEq(tabs.length, 2);
        assertEq(tabs[0].url, tabs[1].url);
        assertEq(tabs[0].index + 1, tabs[1].index);
      }));
  }
]);
