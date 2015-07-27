// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId_;

chrome.test.runTests([
  function setupWindow() {
    chrome.tabs.getCurrent(pass(function(tab) {
      testTabId_ = tab.id;
    }));
  },

  function mutedStartsFalse() {
    chrome.tabs.get(testTabId_, pass(function(tab) {
      assertEq(false, tab.muted);

      queryForTab(testTabId_, {muted: false}, pass(function(tab) {
        assertEq(false, tab.muted);
      }));
      queryForTab(testTabId_, {muted: true} , pass(function(tab) {
        assertEq(null, tab);
      }));
    }));
  },

  function makeMuted() {
    onUpdatedExpect("muted", true, {mutedCause: chrome.runtime.id});
    chrome.tabs.update(testTabId_, {muted: true}, pass());
  },

  function testStaysMutedAfterChangingWindow() {
    chrome.windows.create({}, pass(function(window) {
      chrome.tabs.move(testTabId_, {windowId: window.id, index: -1},
                       pass(function(tab) {
        assertEq(true, tab.muted);
      }));
    }));
  },

  function makeNotMuted() {
    onUpdatedExpect("muted", false, {mutedCause: chrome.runtime.id});
    chrome.tabs.update(testTabId_, {muted: false}, pass());
  }
]);
