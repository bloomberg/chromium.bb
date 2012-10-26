// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.experimental.usb;

var tests = [
  function explicitCloseDevice() {
    usb.findDevices(0, 0, {}, function(devices) {
      usb.closeDevice(devices[0]);
      chrome.test.succeed();
    });
  },
];

chrome.test.runTests(tests);
