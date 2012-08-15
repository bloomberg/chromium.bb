// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.experimental.usb;

function createErrorTest(resultCode, errorMessage) {
  return function() {
    var handler = new Object();
    handler.onEvent = (function (callback) {
      return function (usbEvent) {
        chrome.test.assertTrue(resultCode == usbEvent.resultCode);
        chrome.test.assertTrue(errorMessage == usbEvent.error);
        callback();
      }
    })(chrome.test.callbackAdded());

    usb.findDevice(0, 0, handler, function(device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 1;
      transfer.data = new ArrayBuffer(0);
      usb.bulkTransfer(device, transfer);
    });
  };
}

var tests = [
  createErrorTest(0, undefined),
  createErrorTest(1, "Transfer failed"),
  createErrorTest(2, "Transfer timed out"),
];

chrome.test.runTests(tests);
