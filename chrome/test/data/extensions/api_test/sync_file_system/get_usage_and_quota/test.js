// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(calvinlo): Update test code after default quota is made const
// (http://crbug.com/155488).
chrome.test.runTests([
  function getUsageAndQuota() {
    chrome.syncFileSystem.getUsageAndQuota(
        'drive',
        chrome.test.callbackPass(function(info) {
            chrome.test.assertEq(0, info.usage);
            chrome.test.assertEq(123456789, info.quota);
        }));
  }
]);
