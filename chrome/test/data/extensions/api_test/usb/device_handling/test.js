// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.experimental.usb;

var tests = [
  function implicitCloseDevice() {
    usb.findDevice(0, 0, {}, function(device) {
      chrome.test.succeed();
    });
  },
  function explicitCloseDevice() {
    usb.findDevice(0, 0, {}, function(device) {
      usb.closeDevice(device);
      chrome.test.succeed();
    });
  },
];

chrome.test.runTests(tests);
