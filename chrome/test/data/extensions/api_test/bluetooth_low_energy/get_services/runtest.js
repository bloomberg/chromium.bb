// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetServices() {
  chrome.test.assertEq(2, services.length);

  chrome.test.assertEq('service_id0', services[0].instanceId);
  chrome.test.assertEq('00001234-0000-1000-8000-00805f9b34fb',
                       services[0].uuid);
  chrome.test.assertEq(true , services[0].isPrimary);
  chrome.test.assertEq(false, services[0].isLocal);
  chrome.test.assertEq(deviceAddress, services[0].deviceAddress);

  chrome.test.assertEq('service_id1', services[1].instanceId);
  chrome.test.assertEq('00005678-0000-1000-8000-00805f9b34fb',
                       services[1].uuid);
  chrome.test.assertEq(false , services[1].isPrimary);
  chrome.test.assertEq(false, services[1].isLocal);
  chrome.test.assertEq(deviceAddress, services[1].deviceAddress);

  chrome.test.succeed();
}

var deviceAddress = '11:22:33:44:55:66';
var services = null;

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetoothLowEnergy.getServices(deviceAddress, function(result) {
  if (result || !chrome.runtime.lastError) {
    chrome.test.fail('Unexpected device.');
  }

  chrome.bluetoothLowEnergy.getServices(deviceAddress, function(result) {
    failOnError();
    if (!result || result.length != 0) {
      chrome.test.fail('Services should be empty.');
    }

    chrome.bluetoothLowEnergy.getServices(deviceAddress, function(result) {
      failOnError();
      services = result;

      chrome.test.sendMessage('ready', function(message) {
        chrome.test.runTests([testGetServices]);
      });
    });
  });
});
