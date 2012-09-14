// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var attachedDeviceId = '42';

chrome.mediaGalleriesPrivate.onDeviceAttached.addListener(function(details) {
  // Save the device id for the detach test.
  attachedDeviceId = details.deviceId;
  // The C++ test code will check if this is ok.
  chrome.test.sendMessage('attach_test_ok,' + details.deviceName);
});

chrome.mediaGalleriesPrivate.onDeviceDetached.addListener(function(details) {
  // The C++ test does not know the device id, so check here.
  if (details.deviceId != attachedDeviceId) {
    chrome.test.fail('Bad device id in onDeviceDetached test.');
    return;
  }
  chrome.test.sendMessage('detach_test_ok');
});
