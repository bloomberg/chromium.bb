// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.infobars.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Infobars

const assertEq = chrome.test.assertEq;

var windowA = 0;
var windowB = 0;
var tabA = 0;
var tabB = 0;

var tests = [
  function createInfobars() {
    // Only background page should be active at the start.
    assertEq(1, chrome.extension.getViews().length);

    // Get the current tab and window (window A), then create a new
    // one (window B).
    chrome.tabs.getSelected(null, function(tab) {
      tabA = tab.id;
      windowA = tab.windowId;
      console.log('tabid: ' + tabA + ' windowA: ' + windowA);

      chrome.windows.create({"url": "about:blank"}, function(window) {
        windowB = window.id;
        console.log('windowB: ' + windowB);

        // Show infobarA in window A (tab A) (and specify no callback).
        chrome.infobars.show({"path": "infobarA.html", "tabId": tabA});
        // Flow continues in infobarCallbackA.
      });
    });
  }
];

function infobarCallbackA() {
  // We have now added an infobar so the total count goes up one.
  assertEq(2, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({"type": "infobar"}).length);
  // Window A should have 1 infobar.
  assertEq(1, chrome.extension.getViews({"type": "infobar",
                                         "windowId": windowA}).length);
  // Window B should have no infobars.
  assertEq(0, chrome.extension.getViews({"type": "infobar",
                                         "windowId": windowB}).length);

  chrome.tabs.getAllInWindow(windowB, function(tabs) {
    assertEq(1, tabs.length);
    tabB = tabs[0].id;

    // Show infobarB in (current) window B (with callback).
    chrome.infobars.show({"path": "infobarB.html", "tabId": tabB},
                          function(window) {
      assertEq(window.id, windowB);
      // This infobar will call back to us through infobarCallbackB (below).
    });
  });
}

function infobarCallbackB() {
  // We have now added an infobar so the total count goes up one.
  assertEq(3, chrome.extension.getViews().length);
  assertEq(2, chrome.extension.getViews({"type": "infobar"}).length);

  // Window A should have 1 infobar.
  assertEq(1, chrome.extension.getViews({"type": "infobar",
                                         "windowId": windowA}).length);
  // Window B should have 1 infobar.
  assertEq(1, chrome.extension.getViews({"type": "infobar",
                                         "windowId": windowB}).length);

  chrome.test.notifyPass();
}

chrome.test.runTests(tests);
