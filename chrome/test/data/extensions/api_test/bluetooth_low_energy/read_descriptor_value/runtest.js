// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testReadDescriptorValue() {
  chrome.test.assertTrue(descriptor != null, '\'descriptor\' is null');
  chrome.test.assertEq(descId, descriptor.instanceId);

  chrome.test.succeed();
}

var readDescriptorValue = chrome.bluetoothLowEnergy.readDescriptorValue;
var descId = 'desc_id0';
var badDescId = 'desc_id1';

var descriptor = null;

// 1. Unknown descriptor instanceId.
readDescriptorValue(badDescId, function (result) {
  if (result || !chrome.runtime.lastError) {
    chrome.test.fail('\'badDescId\' did not cause failure');
  }

  // 2. Known descriptor instanceId, but call failure.
  readDescriptorValue(descId, function (result) {
    if (result || !chrome.runtime.lastError) {
      chrome.test.fail('readDescriptorValue should have failed');
    }

    // 3. Call should succeed.
    readDescriptorValue(descId, function (result) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(chrome.runtime.lastError.message);
      }

      descriptor = result;

      chrome.test.sendMessage('ready', function (message) {
        chrome.test.runTests([testReadDescriptorValue]);
      });
    });
  });
});
