// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function getDevices() {
  usb.getDevices({
      vendorId: 0,
      productId: 0
  }, function(devices) {
    chrome.test.assertEq(1, devices.length);
    var device = devices[0];
    chrome.test.assertEq("Test Device", device.productName);
    chrome.test.assertEq("Test Manufacturer", device.manufacturerName);
    chrome.test.assertEq("ABC123", device.serialNumber);
    usb.openDevice(device, function(handle) {
      chrome.test.assertNoLastError();
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
