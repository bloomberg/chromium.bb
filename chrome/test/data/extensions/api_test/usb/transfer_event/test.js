// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.experimental.usb;

function handler() {
  var handlerObject = new Object();
  handlerObject.onEvent = chrome.test.callbackAdded();
  return handlerObject;
}

var tests = [
  function controlTransfer() {
    usb.findDevice(0, 0, handler(), function (device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.recipient = "device";
      transfer.requestType = "standard";
      transfer.request = 1;
      transfer.value = 2;
      transfer.index = 3;
      transfer.data = new ArrayBuffer(1);

      usb.controlTransfer(device, transfer);
    });
  },
  function bulkTransfer() {
    usb.findDevice(0, 0, handler(), function (device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 1;
      transfer.data = new ArrayBuffer(1);

      usb.bulkTransfer(device, transfer);
    });
  },
  function interruptTransfer() {
    usb.findDevice(0, 0, handler(), function (device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 2;
      transfer.data = new ArrayBuffer(1);

      usb.interruptTransfer(device, transfer);
    });
  },
  function isochronousTransfer() {
    usb.findDevice(0, 0, handler(), function (device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 3;
      transfer.data = new ArrayBuffer(1);

      var isoTransfer = new Object();
      isoTransfer.transferInfo = transfer;
      isoTransfer.packets = 1;
      isoTransfer.packetLength = 1;

      usb.isochronousTransfer(device, isoTransfer);
    });
  },
];

chrome.test.runTests(tests);
