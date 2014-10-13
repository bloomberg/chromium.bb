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

var queue = [];

function runNext(result) {
  if (queue.length == 0)
    chrome.test.fail("No more tests!");

  (queue.shift())(result);
}

var errorFailed = 'Operation failed';
var errorInProgress = 'In progress';
var errorInvalidLength = 'Invalid data length';
var errorPermissionDenied = 'Permission denied';
var errorNotConnected = 'Not connected';
var errorGattNotSupported = 'Operation not supported by this service';
var errorNotNotifying = 'Not notifying';
var errorNotFound = 'Instance not found';
var errorOperationFailed = 'Operation failed';
var errorAuthenticationFailed = 'Authentication failed';
var errorCanceled = 'Request canceled';
var errorTimeout = 'Operation timed out';
var errorUnsupportedDevice =
    'This device is not supported on the current platform';
var errorPlatformNotSupported =
    'This operation is not supported on the current platform';

function makeExpectedErrorCallback(expectedError) {
  return function(result) {
    console.log('Expecting error ' + expectedError);
    if (result || !chrome.runtime.lastError ||
        chrome.runtime.lastError.message != expectedError) {
      errorMsg = 'readDescriptorValue expected error \'' + expectedError + '\'';
      if (chrome.runtime.lastError) {
        errorMsg = errorMsg + ' but got \'' + chrome.runtime.lastError.message +
            '\'';
      }
      earlyError(errorMsg);
      return;
    }

    readDescriptorValue(descId, runNext);
  };
}

queue = [function () {
  // 1. Unknown descriptor instanceId.
  readDescriptorValue(badDescId, runNext);
}, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('\'badDescId\' did not cause failure');
    return;
  }

  // 2. Known descriptor instanceId, but call failure.
  readDescriptorValue(descId, runNext);
},
  makeExpectedErrorCallback(errorFailed),
  makeExpectedErrorCallback(errorInvalidLength),
  makeExpectedErrorCallback(errorPermissionDenied),
  makeExpectedErrorCallback(errorPermissionDenied),
  makeExpectedErrorCallback(errorNotConnected),
  makeExpectedErrorCallback(errorGattNotSupported),
  makeExpectedErrorCallback(errorInProgress),
  function (result) {
  if (chrome.runtime.lastError) {
    earlyError(chrome.runtime.lastError.message);
    return;
  }

  descriptor = result;

  chrome.test.sendMessage('ready', function (message) {
    chrome.test.runTests([testReadDescriptorValue]);
  });
}];

runNext();

