// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function getDevices() {
  usb.getDevices({
      vendorId: 0,
      productId: 0
  }, function(devices) {
    usb.openDevice(devices[0], function(handle) {
      usb.closeDevice(handle);
      chrome.test.succeed();
    });
  });
}

function explicitCloseDevice() {
  usb.findDevices({
      vendorId: 0,
      productId: 0
  }, function(devices) {
    usb.closeDevice(devices[0]);
    chrome.test.succeed();
  });
}

chrome.test.runTests([
    getDevices, explicitCloseDevice
]);
