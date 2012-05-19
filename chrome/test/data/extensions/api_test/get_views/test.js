// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.getViews.
// browser_tests.exe --gtest_filter=ExtensionApiTest.GetViews

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

// We need to remember the popupWindowId to be able to find it later.
var popupWindowId = 0;

// This function is called by the popup during the test run.
function popupCallback() {
  // We have now added an popup so the total count goes up one.
  assertEq(2, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({"windowId": popupWindowId}).length);

  chrome.tabs.create({"url": chrome.extension.getURL("options.html")});
}

function optionsPageCallback() {
  assertEq(3, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({"windowId": popupWindowId}).length);
  assertEq(2, chrome.extension.getViews(
      {"type": "tab", "windowId": window.id}).length);
  chrome.test.notifyPass();
}

var tests = [
  function getViews() {
    assertTrue(typeof(chrome.extension.getBackgroundPage()) != "undefined");
    assertEq(1, chrome.extension.getViews().length);
    assertEq(0, chrome.extension.getViews({"type": "tab"}).length);
    assertEq(0, chrome.extension.getViews({"type": "popup"}).length);
    assertEq(0, chrome.extension.getViews({"type": "notification"}).length);

    chrome.windows.getAll({populate: true}, function(windows) {
      assertEq(1, windows.length);

      // Create a popup window.
      chrome.windows.create({"url": chrome.extension.getURL("popup.html"),
                             "type": "popup"}, function(window) {
        assertTrue(window.id > 0);
        popupWindowId = window.id;
        // The popup will call back to us through popupCallback (above).
      });
    });
  }
];

chrome.test.runTests(tests);
