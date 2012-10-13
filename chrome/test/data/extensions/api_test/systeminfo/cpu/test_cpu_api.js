// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.cpu api test
// browser_tests.exe --gtest_filter=SystemInfoCpuApiTest.*

chrome.systemInfo = chrome.experimental.systemInfo;

var userStep = 3;
var kernelStep = 2;
var idleStep = 1;
function calculateUsage(count) {
  return (100 - idleStep * 100/(userStep + kernelStep + idleStep));
}

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 20; ++i) {
      chrome.systemInfo.cpu.get(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4, result.numOfProcessors);
        chrome.test.assertEq("x86", result.archName);
        chrome.test.assertEq("unknown", result.modelName);
      }));
    }
  },

  function testUpdatedEvent() {
    var numOfUpdatedEvent = 0;
    var doneUpdatedEvent = chrome.test.listenForever(
      chrome.systemInfo.cpu.onUpdated,
      function listener(updateInfo) {
        var expectedUsage = calculateUsage(numOfUpdatedEvent);
        chrome.test.assertEq(updateInfo.averageUsage, expectedUsage);

        chrome.test.assertEq(updateInfo.usagePerProcessor.length, 4);
        for (var i = 0; i < updateInfo.usagePerProcessor.length; ++i)
          chrome.test.assertEq(updateInfo.usagePerProcessor[i], expectedUsage);
        if (++numOfUpdatedEvent > 5)
          doneUpdatedEvent();
      });
  }
]);

