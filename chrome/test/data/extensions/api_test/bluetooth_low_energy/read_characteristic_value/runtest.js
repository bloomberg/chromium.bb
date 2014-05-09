// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testReadCharacteristicValue() {
  chrome.test.assertTrue(characteristic != null, '\'characteristic\' is null');
  chrome.test.assertEq(charId, characteristic.instanceId);

  chrome.test.succeed();
}

var readCharacteristicValue = chrome.bluetoothLowEnergy.readCharacteristicValue;
var charId = 'char_id0';
var badCharId = 'char_id1';

var characteristic = null;

// 1. Unknown characteristic instanceId.
readCharacteristicValue(badCharId, function (result) {
  if (result || !chrome.runtime.lastError) {
    chrome.test.fail('\'badCharId\' did not cause failure');
  }

  // 2. Known characteristic instanceId, but call failure.
  readCharacteristicValue(charId, function (result) {
    if (result || !chrome.runtime.lastError) {
      chrome.test.fail('readCharacteristicValue should have failed');
    }

    // 3. Call should succeed.
    readCharacteristicValue(charId, function (result) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(chrome.runtime.lastError.message);
      }

      characteristic = result;

      chrome.test.sendMessage('ready', function (message) {
        chrome.test.runTests([testReadCharacteristicValue]);
      });
    });
  });
});
