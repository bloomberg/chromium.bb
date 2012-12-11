// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('ready', function(message) {
  chrome.test.runTests([
    function testPermission() {
      if (chrome.dial)
        chrome.test.fail();
      else
        chrome.test.succeed();
    }
  ]);
});
