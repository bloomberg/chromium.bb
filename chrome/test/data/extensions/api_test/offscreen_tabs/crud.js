// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabList = [
  { url: 'a.html', width: 200, height: 200 },
  { url: 'b.html', width: 200, height: 200 },
  { url: 'c.html', width: 1000,height: 800 }
];

// Holds the tab objects once they're created.
var tabs = [];

chrome.test.runTests([
  function create() {
    tabList.forEach(function(tab) {
      chrome.experimental.offscreenTabs.create(
          tab, pass(function(offscreenTab) {
        tabs.push(offscreenTab);
        assertSimilarTabs(tab, offscreenTab);
      }));
    });
  },

  function getAll() {
    chrome.experimental.offscreenTabs.getAll(pass(function(offscreenTabs) {
      var tabSorter = function(tab1, tab2) { return tab1.id - tab2.id; };
      offscreenTabs.sort(tabSorter);
      tabs.sort(tabSorter);
      assertEq(tabs.length, offscreenTabs.length);
      assertEq(tabs.length, tabList.length);
      for (var i = 0; i < tabs.length; i++)
        assertEqTabs(tabs[i], offscreenTabs[i]);
    }));
  },

  function get() {
    chrome.experimental.offscreenTabs.get(tabs[0].id, pass(function(tab) {
      assertEqTabs(tabs[0], tab);
    }));
  },

  function update() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      assertSimilarTabs(tabList[0], tab);
    });

    // Update the 2nd tab to be like the 1st one.
    chrome.experimental.offscreenTabs.update(
        tabs[1].id, tabList[0], pass(function() {
      chrome.experimental.offscreenTabs.get(tabs[1].id, pass(function(tab) {
        assertSimilarTabs(tabList[0], tab);
      }));
    }));
  },

  function remove() {
    tabs.forEach(function(tab) {
      chrome.experimental.offscreenTabs.remove(tab.id, pass(function() {
        assertTabDoesNotExist(tab.id);
      }));
    });
  }
]);
