// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTab = { url: 'a.html', width: 200, height: 200 };
var testTabId;

chrome.test.runTests([
  function init() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      assertSimilarTabs(testTab, tab);
    });

    chrome.experimental.offscreenTabs.create(testTab, pass(function(tab) {
      assertSimilarTabs(testTab, tab);
      testTabId = tab.id;
    }));
  },

  // Test that keyboard events work by sending a 'q' keypress to a.html. The
  // page has a keypress handler that will navigate the page to c.html.
  function keyPress() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      testTab.url = 'c.html';
      assertEq(maybeExpandURL('c.html'), changeInfo.url);
      assertSimilarTabs(testTab, tab);
      assertEq(tabId, tab.id);
      assertEq(tabId, testTabId);
    });

    chrome.experimental.offscreenTabs.sendKeyboardEvent(
        testTabId, getKeyboardEvent(Q_KEY), pass(function() {
      chrome.experimental.offscreenTabs.get(testTabId, pass(function(tab) {
        assertSimilarTabs(testTab, tab);
      }));
    }));
  }
]);
