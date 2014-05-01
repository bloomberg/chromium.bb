// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetIncludedServices() {
  chrome.test.assertTrue(services != null, '\'services\' is null');
  chrome.test.assertEq(1, services.length);
  chrome.test.assertEq(includedId, services[0].instanceId);

  chrome.test.succeed();
}

var serviceId = 'service_id0';
var includedId = 'service_id1';
var services = null;

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetoothLowEnergy.getIncludedServices(serviceId, function (result) {
  // No mapping for |serviceId|.
  if (result || !chrome.runtime.lastError) {
    chrome.test.fail('getIncludedServices should have failed');
  }

  chrome.test.sendMessage('ready', function (message) {
    chrome.bluetoothLowEnergy.getIncludedServices(serviceId, function (result) {
      failOnError();
      if (!result || result.length != 0) {
        chrome.test.fail('Included services should be empty.');
      }

      chrome.bluetoothLowEnergy.getIncludedServices(serviceId,
          function (result) {
            failOnError();
            services = result;

            chrome.test.sendMessage('ready', function (message) {
              chrome.test.runTests([testGetIncludedServices]);
            });
      });
    });
  });
});
