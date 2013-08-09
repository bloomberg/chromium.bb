// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.storage api test
// browser_tests --gtest_filter=SystemStorageApiTest.Storage

// Testing data should be the same as |kTestingData| in
// system_storage_apitest.cc.
var testData = [
  { id:"", name: "0xbeaf", type: "removable", capacity: 4098 },
  { id:"", name: "/home", type: "fixed", capacity: 4098 },
  { id:"", name: "/data", type: "fixed", capacity: 10000 }
];

chrome.test.runTests([
  function testGet() {
    chrome.system.storage.getInfo(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 3);
      for (var i = 0; i < units.length; ++i) {
        chrome.test.sendMessage(units[i].id);
        chrome.test.assertEq(testData[i].name, units[i].name);
        chrome.test.assertEq(testData[i].type, units[i].type);
        chrome.test.assertEq(testData[i].capacity, units[i].capacity);
      }
    }));
  }
]);
