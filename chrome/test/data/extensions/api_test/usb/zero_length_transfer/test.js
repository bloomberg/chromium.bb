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
  function zeroLengthTransfer() {
    usb.findDevice(0, 0, handler(), function(device) {
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 1;
      transfer.data = new ArrayBuffer(0);
      usb.bulkTransfer(device, transfer);
    });
  },
];

chrome.test.runTests(tests);
