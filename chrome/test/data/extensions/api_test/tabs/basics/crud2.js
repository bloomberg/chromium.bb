// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var secondWindowId;
var thirdWindowId;
var testTabId;

chrome.test.runTests([

  function setupTwoWindows() {
    createWindow(["about:blank", "chrome://newtab/", pageUrl("a")], {},
                pass(function(winId, tabIds) {
      secondWindowId = winId;
      testTabId = tabIds[2];

      createWindow(["chrome://newtab/", pageUrl("b")], {},
                           pass(function(winId, tabIds) {
        thirdWindowId = winId;
      }));
    }));
  },

  function getAllInWindow() {
    chrome.tabs.getAllInWindow(secondWindowId,
                               pass(function(tabs) {
      assertEq(3, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(secondWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);

        // The first tab should be active
        assertEq((i == 0), tabs[i].active && tabs[i].selected);
      }
      assertEq("about:blank", tabs[0].url);
      assertEq("chrome://newtab/", tabs[1].url);
      assertEq(pageUrl("a"), tabs[2].url);
    }));

    chrome.tabs.getAllInWindow(thirdWindowId,
                               pass(function(tabs) {
      assertEq(2, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(thirdWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);
      }
      assertEq("chrome://newtab/", tabs[0].url);
      assertEq(pageUrl("b"), tabs[1].url);
    }));
  },

  function updateSelect() {
    chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
      assertEq(true, tabs[0].active && tabs[0].selected);
      assertEq(false, tabs[1].active || tabs[1].selected);
      assertEq(false, tabs[2].active || tabs[2].selected);

      // Select tab[1].
      chrome.tabs.update(tabs[1].id, {active: true},
                         pass(function(tab1){
        // Check update of tab[1].
        chrome.test.assertEq(true, tab1.active);
        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(true, tabs[1].active && tabs[1].selected);
          assertEq(false, tabs[2].active || tabs[2].selected);
          // Select tab[2].
          chrome.tabs.update(tabs[2].id,
                             {active: true},
                             pass(function(tab2){
            // Check update of tab[2].
            chrome.test.assertEq(true, tab2.active);
            chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
              assertEq(false, tabs[1].active || tabs[1].selected);
              assertEq(true, tabs[2].active && tabs[2].selected);
            }));
          }));
        }));
      }));
    }));
  },

  function update() {
    chrome.tabs.get(testTabId, pass(function(tab) {
      assertEq(pageUrl("a"), tab.url);
      // Update url.
      chrome.tabs.update(testTabId, {"url": pageUrl("c")},
                         pass(function(tab){
        chrome.test.assertEq(pageUrl("c"), tab.url);
        // Check url.
        chrome.tabs.get(testTabId, pass(function(tab) {
          assertEq(pageUrl("c"), tab.url);
        }));
      }));
    }));
  },

]);
