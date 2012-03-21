// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTab = { url: 'a.html', width: 200, height: 200 };
var testTabId;

var linkX = 11;
var linkY = 11;

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

  // Test that mouse click events work by watching the tab navigate to b.html
  // after clicking the link on a.html.
  function mouseClick() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      testTab.url = 'b.html';
      assertEq(maybeExpandURL('b.html'), changeInfo.url);
      assertEq(testTabId, tabId);
      assertEq(testTabId, tab.id);
      assertSimilarTabs(testTab, tab);
    });

    var mouseEvent = getBaseMouseEvent();
    mouseEvent.type = 'click';

    chrome.experimental.offscreenTabs.sendMouseEvent(
        testTabId, mouseEvent, {x: linkX, y: linkY}, pass(function() {
      chrome.experimental.offscreenTabs.get(testTabId, pass(function(tab) {
        assertEq(testTabId, tab.id);
        assertSimilarTabs(testTab, tab);
      }));
    }));
  },

  // Scroll the b.html page down 100 px so that the link is at (10,10).
  function mouseWheel() {
    var mouseEvent = getBaseMouseEvent();
    mouseEvent.type = 'mousewheel';
    mouseEvent.wheelDeltaX = 0;
    mouseEvent.wheelDeltaY = -100;
    chrome.experimental.offscreenTabs.sendMouseEvent(
        testTabId, mouseEvent, pass(function() {
      chrome.experimental.offscreenTabs.get(testTabId, pass(function(tab) {
        assertSimilarTabs(testTab, tab);
      }));
    }));
  },

  // Now that we scrolled the links into position, sending consecutive
  // mousedown|up events to the link should navigate the page to c.html.
  function mouseDownUp() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      testTab.url = 'c.html';
      assertEq(testTabId, tabId);
      assertEq(testTabId, tab.id);
      assertSimilarTabs(testTab, tab);
    });

    var mouseEvent = getBaseMouseEvent();
    mouseEvent.type = 'mousedown';
    chrome.experimental.offscreenTabs.sendMouseEvent(
        testTabId, mouseEvent, {x: linkX, y: linkY}, pass(function() {
      chrome.experimental.offscreenTabs.get(testTabId, pass(function(tab) {
        assertSimilarTabs(testTab, tab);
      }));
    }));

    mouseEvent.type = 'mouseup';
    chrome.experimental.offscreenTabs.sendMouseEvent(
        testTabId, mouseEvent, {x: linkX, y: linkY}, pass(function() {
      chrome.experimental.offscreenTabs.get(testTabId, pass(function(tab) {
        assertSimilarTabs(testTab, tab);
      }));
    }));
  }
]);
