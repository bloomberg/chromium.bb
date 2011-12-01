// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;
var secondWindowId;
var moveTabIds = {};

chrome.test.runTests([
  // Do a series of moves and removes so that we get the following
  //
  // Before:
  //  Window1: (newtab),a,b,c,d,e
  //  Window2: (newtab)
  //
  // After:
  //  Window1: (newtab),a
  //  Window2: b,(newtab)
  function setupLetterPages() {
    var pages = ["chrome://newtab/", pageUrl('a'), pageUrl('b'),
                   pageUrl('c'), pageUrl('d'), pageUrl('e')];
    createWindow(pages, {}, pass(function(winId, tabIds) {
      firstWindowId = winId;
      moveTabIds['a'] = tabIds[1];
      moveTabIds['b'] = tabIds[2];
      moveTabIds['c'] = tabIds[3];
      moveTabIds['d'] = tabIds[4];
      moveTabIds['e'] = tabIds[5];
      createWindow(["chrome://newtab/"], {}, pass(function(winId, tabIds) {
        secondWindowId = winId;
      }));
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(pages.length, tabs.length);
        for (var i in tabs) {
          assertEq(pages[i], tabs[i].url);
        }
      }));
    }));
  },

  function move() {
    // Check that the tab/window state is what we expect after doing moves.
    function checkMoveResults() {
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(4, tabs.length);
        assertEq("chrome://newtab/", tabs[0].url);
        assertEq(pageUrl("a"), tabs[1].url);
        assertEq(pageUrl("e"), tabs[2].url);
        assertEq(pageUrl("c"), tabs[3].url);

        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(3, tabs.length);
          assertEq(pageUrl("b"), tabs[0].url);
          assertEq("chrome://newtab/", tabs[1].url);
          assertEq(pageUrl("d"), tabs[2].url);
        }));
      }));
    }

    chrome.tabs.move(moveTabIds['b'], {"windowId": secondWindowId, "index": 0},
                     pass(function(tabB) {
        chrome.test.assertEq(0, tabB.index);
        chrome.tabs.move(moveTabIds['e'], {"index": 2},
                         pass(function(tabE) {
          chrome.test.assertEq(2, tabE.index);
          chrome.tabs.move(moveTabIds['d'], {"windowId": secondWindowId,
                           "index": 2}, pass(function(tabD) {
            chrome.test.assertEq(2, tabD.index);
            checkMoveResults();
        }));
      }));
    }));
  },

  function remove() {
    chrome.tabs.remove(moveTabIds["d"], pass(function() {
      chrome.tabs.getAllInWindow(secondWindowId,
                                 pass(function(tabs) {
        assertEq(2, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
      }));
    }));
  },

  function moveMultipleTabs() {
    chrome.tabs.move([moveTabIds['e'], moveTabIds['c']],
                     {windowId: secondWindowId, index: 1},
                     pass(function(tabsA) {
      assertEq(2, tabsA.length);
      assertEq(secondWindowId, tabsA[0].windowId);
      assertEq(pageUrl('e'), tabsA[0].url);
      assertEq(1, tabsA[0].index);
      assertEq(secondWindowId, tabsA[1].windowId);
      assertEq(pageUrl('c'), tabsA[1].url);
      assertEq(2, tabsA[1].index);
      chrome.tabs.query({windowId: secondWindowId}, pass(function(tabsB) {
        assertEq(4, tabsB.length);
      }));
    }));
  },

  function removeMultipleTabs() {
    chrome.tabs.remove([moveTabIds['e'], moveTabIds['c']], pass(function() {
      chrome.tabs.query({windowId: secondWindowId}, pass(function(tabs) {
        assertEq(2, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
      }));
    }));
  },

  // Make sure we don't crash when the index is out of range.
  function moveToInvalidTab() {
    var error_msg = "Invalid value for argument 2. Property 'index': " +
        "Value must not be less than 0.";
    try {
      chrome.tabs.move(moveTabIds['b'], {index: -1}, function(tab) {
        chrome.test.fail("Moved a tab to an invalid index");
      });
    } catch (e) {
      assertEq(error_msg, e.message);
    }
    chrome.tabs.move(moveTabIds['b'], {index: 10000}, pass(function(tabB) {
      assertEq(1, tabB.index);
    }));
  }
]);
