// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systemInfo.storage api test
// browser_tests.exe --gtest_filter=SystemInfoStorageApiTest.Storage
chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    chrome.systemInfo.storage.get(chrome.test.callbackPass(function(info) {
      // TODO(hmin): Need to check the value from Mock implementation
      chrome.test.assertTrue(info.units.length > 0);
    }));
  }
]);
