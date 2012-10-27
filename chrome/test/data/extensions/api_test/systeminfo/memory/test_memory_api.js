// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.memory api test
// browser_tests --gtest_filter=SystemInfoMemoryApiTest.*

chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.systemInfo.memory.get(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4096, result.capacity);
        chrome.test.assertEq(1024, result.availableCapacity);
      }));
    }
  }
]);

