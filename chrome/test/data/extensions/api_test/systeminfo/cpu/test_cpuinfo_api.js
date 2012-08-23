// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.cpu api test
// browser_tests.exe --gtest_filter=SystemInfoCpuApiTest.*

chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 20; ++i) {
      chrome.systemInfo.cpu.get(chrome.test.callbackPass(function(result) {
        chrome.test.assertTrue(result.cores.length == 4);
        var core;
        for (var i = 0; i < result.cores.length; ++i) {
          core = result.cores[i];
          chrome.test.assertTrue(core.load == i*10);
        }
      }));
    }
  },
]);

