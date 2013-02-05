// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systemInfo.storage api test
// browser_tests --gtest_filter=SystemInfoStorageApiTest.Storage
chrome.systemInfo = chrome.experimental.systemInfo;

// Testing data should be the same as that in system_info_storage_apitest.cc
var testData = [
  { id: "0xbeaf", type: "unknown", capacity: 4098,
    availableCapacity: 1000, step: 0 },
  { id: "/home", type: "harddisk", capacity: 4098,
    availableCapacity: 1000, step: 10 },
  { id: "/data", type: "harddisk", capacity: 10000,
    availableCapacity: 1000, step: 4097 }
];

chrome.test.runTests([
  function testGet() {
    chrome.systemInfo.storage.get(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 3);
      for (var i = 0; i < units.length; ++i) {
        chrome.test.assertEq(testData[i].id, units[i].id);
        chrome.test.assertEq(testData[i].type, units[i].type);
        chrome.test.assertEq(testData[i].capacity, units[i].capacity);
        chrome.test.assertEq(testData[i].availableCapacity,
                             units[i].availableCapacity);
        testData[i].availableCapacity += testData[i].step;
      }
    }));
  },
  function testChangedEvent() {
    var numOfChangedEvent = 0;
    var callbackCompleted = chrome.test.callbackAdded();
    chrome.systemInfo.storage.onAvailableCapacityChanged.addListener(
      function listener(changeInfo) {
        for (var i = 0; i < testData.length; ++i) {
          if (changeInfo.id == testData[i].id) {
            // Increase its availableCapacity since it will be queried before
            // triggering onChanged event.
            testData[i].availableCapacity += testData[i].step;
            chrome.test.assertEq(testData[i].availableCapacity,
                                  changeInfo.availableCapacity);
            break;
          }
        }

        if (i >= testData.length)
          chrome.test.fail("No matched storage id is found!");

        if (++numOfChangedEvent > 10) {
          chrome.systemInfo.storage.onAvailableCapacityChanged.removeListener(
            listener);
          setTimeout(callbackCompleted, 0);
        }
     });
   }
]);
