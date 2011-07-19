// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function simple() {
    try {
      chrome.contextMenus.create({"title":"1"}, function() {
      });
    } catch (e) {
      chrome.test.assertTrue(e.message.indexOf(
          "You do not have permission to use 'contextMenus.create'") == 0);
      chrome.test.succeed();
      return;
    }

    chrome.test.fail("Should have gotten access error.");
  }
];

chrome.test.runTests(tests);
