// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systemInfo.storage api test
// browser_tests --gtest_filter=SystemInfoStorageApiTest.Storage
chrome.systemInfo = chrome.experimental.systemInfo;

// Testing data should be the same as that in system_info_storage_apitest.cc
var testData = {
  id: "/media/usb1",
  type: "removable",
  capacity: 4098
};

chrome.test.runTests([
  function testAttachedEvent() {
    chrome.test.listenOnce(
      chrome.systemInfo.storage.onAttached,
      function listener(info) {
        chrome.test.assertEq(testData.id, info.id);
        chrome.test.assertEq(testData.type, info.type);
        chrome.test.assertEq(testData.capacity, info.capacity);
      }
    );

    // Tell browser process to attach a new removable storage.
    chrome.test.sendMessage("attach");
  },

  function testDetachedEvent() {
    chrome.test.listenOnce(
      chrome.systemInfo.storage.onDetached,
      function listener(id) {
        chrome.test.assertEq(testData.id, id);
      }
    );
    // Tell browser process to detach a storage.
    chrome.test.sendMessage("detach");
  }
]);
