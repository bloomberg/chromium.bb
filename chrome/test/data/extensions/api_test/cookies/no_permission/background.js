// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function readCookies() {
    try {
      chrome.cookies.get({url: document.location.href, name: 'a'}, function() {
      });
    } catch (e) {
      chrome.test.assertTrue(e.message.indexOf(
          "You do not have permission to use 'cookies.get'.") == 0);
      chrome.test.succeed();
      return;
    }

    chrome.test.fail();
  }
]);
