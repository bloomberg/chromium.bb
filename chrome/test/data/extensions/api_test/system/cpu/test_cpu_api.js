// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.cpu.getInfo api test
// browser_tests.exe --gtest_filter=SystemCpuApiTest.*

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 20; ++i) {
      chrome.system.cpu.getInfo(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4, result.numOfProcessors);
        chrome.test.assertEq("x86", result.archName);
        chrome.test.assertEq("unknown", result.modelName);
      }));
    }
  }
]);

