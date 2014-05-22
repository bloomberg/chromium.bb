// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDescriptor() {
  chrome.test.assertTrue(descriptor != null, '\'descriptor\' is null');

  chrome.test.assertEq('desc_id0', descriptor.instanceId);
  chrome.test.assertEq('00001221-0000-1000-8000-00805f9b34fb', descriptor.uuid);
  chrome.test.assertEq(false, descriptor.isLocal);
  chrome.test.assertEq(charId, descriptor.characteristic.instanceId);

  var valueBytes = new Uint8Array(descriptor.value);
  chrome.test.assertEq(3, descriptor.value.byteLength);
  chrome.test.assertEq(0x01, valueBytes[0]);
  chrome.test.assertEq(0x02, valueBytes[1]);
  chrome.test.assertEq(0x03, valueBytes[2]);

  chrome.test.succeed();
}

var getDescriptor = chrome.bluetoothLowEnergy.getDescriptor;

var charId = 'char_id0';
var descId = 'desc_id0';
var badDescId = 'desc_id1';

var descriptor = null;

// 1. Unknown descriptor instanceId.
getDescriptor(badDescId, function (result) {
  if (result || !chrome.runtime.lastError) {
    chrome.test.fail('getDescriptor should have failed for \'badDescId\'');
  }

  // 2. Known descriptor instanceId, but the mapped device is unknown.
  getDescriptor(descId, function (result) {
    if (result || !chrome.runtime.lastError) {
      chrome.test.fail('getDescriptor should have failed');
    }

    // 3. Known descriptor instanceId, but the mapped service is unknown.
    getDescriptor(descId, function (result) {
      if (result || !chrome.runtime.lastError) {
        chrome.test.fail('getDescriptor should have failed');
      }

      // 4. Known descriptor instanceId, but the mapped characteristic is
      // unknown.
      getDescriptor(descId, function (result) {
        if (result || !chrome.runtime.lastError) {
          chrome.test.fail('getDescriptor should have failed');
        }

        // 5. Known descriptor instanceId, but the mapped the characteristic
        // does not know about the descriptor.
        getDescriptor(descId, function (result) {
          if (result || !chrome.runtime.lastError) {
            chrome.test.fail('getDescriptor should have failed');
          }

          // 6. Success.
          getDescriptor(descId, function (result) {
            if (chrome.runtime.lastError) {
              chrome.test.fail(chrome.runtime.lastError.message);
            }

            descriptor = result;

            chrome.test.sendMessage('ready', function (message) {
              chrome.test.runTests([testGetDescriptor]);
            });
          });
        });
      });
    });
  });
});
