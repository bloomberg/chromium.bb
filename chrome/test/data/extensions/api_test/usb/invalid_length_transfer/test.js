// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var genericTransfer = {
  "direction": "in",
  "endpoint": 1,
  "length": -1
};
var controlTransfer = {
  "index": 0,
  "direction": "in",
  "requestType": "standard",
  "recipient": "device",
  "request": 0,
  "value": 0,
  "length": -1
};
var isoTransfer = {
  "packetLength": 0,
  "transferInfo": genericTransfer,
  "packets": 0
};
var errorMessage = 'Transfer length must be a ' +
    'positive number less than 104,857,600.';
var largeSize = 100 * 1024 * 1024 + 1;

function createInvalidTransferTest(usbFunction, transferInfo, transferLength) {
  return function() {
    genericTransfer["length"] = transferLength;
    controlTransfer["length"] = transferLength;
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      usbFunction(device, transferInfo, chrome.test.callbackFail(
          errorMessage, function() {}));
    });
  };
}

var tests = [
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, -1),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, -1),
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, largeSize),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, largeSize)
];

chrome.test.runTests(tests);
