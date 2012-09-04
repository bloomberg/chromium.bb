// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systemInfo.storage api test
// browser_tests.exe --gtest_filter=SystemInfoStorageApiTest.Storage
chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    chrome.systemInfo.storage.get(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 1);
      var unit = units[0];
      chrome.test.assertTrue(unit.id == "0xbeaf");
      chrome.test.assertTrue(unit.type == "unknown");
      chrome.test.assertTrue(unit.capacity == 4098);
      chrome.test.assertTrue(unit.availableCapacity == 1024);
    }));
  },
  function testChangedEvent() {
    chrome.test.sendMessage("ready", function() {
      var numOfChangedEvent = 0;
      var base = 10000;
      var step = 10;
      var doneChangedEvent = chrome.test.listenForever(
        chrome.systemInfo.storage.onAvailableCapacityChanged,
        function listener(changedInfo) {
          chrome.test.assertTrue(changedInfo.id == "/dev/sda1");
          chrome.test.assertTrue(
            changedInfo.availableCapacity == (base - step*numOfChangedEvent));
          if (++numOfChangedEvent > 5)
            doneChangedEvent();
        });
      });
    }
]);
