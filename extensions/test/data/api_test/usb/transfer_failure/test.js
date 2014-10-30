// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function createErrorTest(resultCode, errorMessage) {
  return function() {
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 1;
      transfer.data = new ArrayBuffer(0);
      usb.bulkTransfer(device, transfer, function (result) {
        chrome.test.assertTrue(resultCode == result.resultCode);
        chrome.test.succeed();
      });
    });
  };
}

var tests = [
  createErrorTest(0, undefined),
  createErrorTest(1, "Transfer failed"),
  createErrorTest(2, "Transfer timed out"),
];

chrome.test.runTests(tests);
