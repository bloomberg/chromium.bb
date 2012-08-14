// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.cpu api test
// browser_tests.exe --gtest_filter=SystemInfoCpuApiTest.*

chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    chrome.systemInfo.cpu.get(chrome.test.callbackPass(function(result){
      chrome.test.assertTrue(result.cores.length == 1);
      var core = result.cores[0];
      chrome.test.assertTrue(core.load == 53);
    }));
  },
]);

