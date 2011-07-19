// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.getViews.
// browser_tests.exe --gtest_filter=ExtensionApiTest.GetViews

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

// This function is called by the infobar during the test run.
function infobarCallback() {
  // We have now added an infobar so the total count goes up one.
  assertEq(2, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({"type": "infobar"}).length);

  // Now try the same, but specify the windowId.
  chrome.windows.getCurrent(function(window) {
    assertEq(1, chrome.extension.getViews(
        {"type": "infobar", "windowId": window.id}).length);
    chrome.tabs.create({"url": chrome.extension.getURL("options.html")});
  });
}

function optionsPageCallback() {
  assertEq(3, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews(
      {"type": "infobar", "windowId": window.id}).length);
  assertEq(1, chrome.extension.getViews(
      {"type": "tab", "windowId": window.id}).length);
  chrome.test.notifyPass();
}

var tests = [
  function getViews() {
    assertTrue(typeof(chrome.extension.getBackgroundPage()) != "undefined");
    assertEq(1, chrome.extension.getViews().length);
    assertEq(0, chrome.extension.getViews({"type": "tab"}).length);
    assertEq(0, chrome.extension.getViews({"type": "infobar"}).length);
    assertEq(0, chrome.extension.getViews({"type": "notification"}).length);
    
    chrome.windows.getAll({populate: true}, function(windows) {
      assertEq(1, windows.length);

      // Show an infobar.
      chrome.experimental.infobars.show({tabId: windows[0].tabs[0].id,
                                         "path": "infobar.html"},
                                        function(window) {
        assertTrue(window.id > 0);
        // The infobar will call back to us through infobarCallback (above).
      });

    });
  }
];

chrome.test.runTests(tests);
