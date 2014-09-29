// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testReadDescriptorValue() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertTrue(descriptor != null, '\'descriptor\' is null');
  chrome.test.assertEq(descId, descriptor.instanceId);

  chrome.test.succeed();
}

var readDescriptorValue = chrome.bluetoothLowEnergy.readDescriptorValue;
var descId = 'desc_id0';
var badDescId = 'desc_id1';

var descriptor = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testReadDescriptorValue]);
}

// 1. Unknown descriptor instanceId.
readDescriptorValue(badDescId, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('\'badDescId\' did not cause failure');
    return;
  }

  // 2. Known descriptor instanceId, but call failure.
  readDescriptorValue(descId, function (result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('readDescriptorValue should have failed');
      return;
    }

    // 3. Call should succeed.
    readDescriptorValue(descId, function (result) {
      if (chrome.runtime.lastError) {
        earlyError(chrome.runtime.lastError.message);
        return;
      }

      descriptor = result;

      chrome.test.sendMessage('ready', function (message) {
        chrome.test.runTests([testReadDescriptorValue]);
      });
    });
  });
});
