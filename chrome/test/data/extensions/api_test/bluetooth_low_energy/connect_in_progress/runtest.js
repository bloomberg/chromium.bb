// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress0 = '11:22:33:44:55:66';
var ble = chrome.bluetoothLowEnergy;

var errorInProgress = 'In progress';

function expectError(message) {
  if (!chrome.runtime.lastError ||
      chrome.runtime.lastError.message != message)
    chrome.test.fail('Expected error: ' + message);
}

function expectSuccess() {
  if (chrome.runtime.lastError)
    chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
}

ble.connect(deviceAddress0, function () {
  expectSuccess();
  ble.disconnect(deviceAddress0, function () {
    chrome.test.succeed();
  });

  ble.disconnect(deviceAddress0, function () {
    expectError(errorInProgress);
    chrome.test.sendMessage('ready');
  });
});

ble.connect(deviceAddress0, function () {
  expectError(errorInProgress);
  chrome.test.sendMessage('ready');
});
