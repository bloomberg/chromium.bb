// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All of the calls to chrome.* functions should fail, since this extension
// has requested no permissions.

chrome.test.runTests([
  function history() {
    try {
      var query = { 'text': '', 'maxResults': 1 };
      chrome.history.search(query, function(results) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  },

  function bookmarks() {
    try {
      chrome.bookmarks.get("1", function(results) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  },

  function tabs() {
    try {
      chrome.tabs.getSelected(null, function(results) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  },

  function idle() {
    try {
      chrome.idle.queryState(60, function(state) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  }
]);
