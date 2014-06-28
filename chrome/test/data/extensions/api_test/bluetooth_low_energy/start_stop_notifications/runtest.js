// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testStartStopNotifications() {
  chrome.test.assertEq(1, Object.keys(changedChrcs).length);
  chrome.test.assertEq(charId0, changedChrcs[charId0].instanceId);
  chrome.test.succeed();
}

var errorAlreadyNotifying = 'Already notifying';
var errorNotFound = 'Instance not found';
var errorNotNotifying = 'Not notifying';
var errorOperationFailed = 'Operation failed';
var errorPermissionDenied = 'Permission denied';

var charId0 = 'char_id0';
var charId1 = 'char_id1';
var charId2 = 'char_id2';

var changedChrcs = {};
var ble = chrome.bluetoothLowEnergy;
var start = ble.startCharacteristicNotifications;
var stop = ble.stopCharacteristicNotifications;

function sendReady(errorMessage) {
  chrome.test.sendMessage('ready', function (message) {
    if (errorMessage) {
      chrome.test.fail(errorMessage);
      return;
    }

    chrome.test.runTests([testStartStopNotifications]);
  });
}

function expectError(expectedMessage) {
  if (!chrome.runtime.lastError) {
    sendReady('Expected error: ' + expectedMessage);
    return;
  }

  if (chrome.runtime.lastError.message != expectedMessage) {
    sendReady('Expected error: ' + expectedMessage + ', got error: ' +
              expectedMessage);
  }
}

function expectSuccess() {
  if (chrome.runtime.lastError) {
    sendReady('Unexpected error: ' + chrome.runtime.lastError.message);
  }
}

ble.onCharacteristicValueChanged.addListener(function (chrc) {
  changedChrcs[chrc.instanceId] = chrc;
});

start('foo', function () {
  expectError(errorNotFound);
  start(charId2, function () {
    expectError(errorPermissionDenied);
    stop(charId0, function () {
      expectError(errorNotNotifying);
      start(charId0, function () {
        expectError(errorOperationFailed);
        start(charId0, function () {
          expectSuccess();
          start(charId0, function () {
            expectError(errorAlreadyNotifying);
            start(charId1, function () {
              expectSuccess();
              stop(charId1, function () {
                expectSuccess();
                stop(charId1, function () {
                  expectError(errorNotNotifying);
                  stop(charId2, function () {
                    expectError(errorNotNotifying);
                    sendReady(undefined);
                  });
                });
              });
            });
          });
        });
      });
    });
  });
});
