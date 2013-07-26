// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.storage api test
// browser_tests --gtest_filter=SystemStorageApiTest.Storage

// Testing data should be the same as |kTestingData| in
// system_storage_apitest.cc.
var testData = [
  { id:"transient:0004", name: "0xbeaf", type: "unknown", capacity: 4098,
    availableCapacity: 1000, step: 0 },
  { id:"transient:002", name: "/home", type: "fixed", capacity: 4098,
    availableCapacity: 1000, step: 10 },
  { id:"transient:003", name: "/data", type: "fixed", capacity: 10000,
    availableCapacity: 1000, step: 4097 }
];

chrome.test.runTests([
  function testGet() {
    chrome.system.storage.getInfo(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 3);
      for (var i = 0; i < units.length; ++i) {
        chrome.test.assertEq(testData[i].id, units[i].id);
        chrome.test.assertEq(testData[i].name, units[i].name);
        chrome.test.assertEq(testData[i].type, units[i].type);
        chrome.test.assertEq(testData[i].capacity, units[i].capacity);
      }
    }));
  },

  function testChangedEvent() {
    var numOfChangedEvent = 0;
    var callbackCompleted = chrome.test.callbackAdded();
    chrome.system.storage.onAvailableCapacityChanged.addListener(
      function listener(changeInfo) {
        for (var i = 0; i < testData.length; ++i) {
          if (changeInfo.id == testData[i].id) {
            chrome.test.assertEq(testData[i].availableCapacity,
                                 changeInfo.availableCapacity);
            // Increase its availableCapacity since it will be queried
            // before triggering onChanged event.
            testData[i].availableCapacity += testData[i].step;
            break;
          }
        }

        if (i >= testData.length)
          chrome.test.fail("No matched storage id is found!");

        if (++numOfChangedEvent > 10) {
          chrome.system.storage.onAvailableCapacityChanged.removeListener(
              listener);
          setTimeout(callbackCompleted, 0);
        }
      });
  }
]);
