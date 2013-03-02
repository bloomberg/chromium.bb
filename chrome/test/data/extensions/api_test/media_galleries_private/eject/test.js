// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var attachedDeviceId = '42';

function testAttach(details) {
  attachedDeviceId = details.deviceId;
  chrome.test.sendMessage('attach_test_ok,' + details.deviceName);
};

function ejectCallback(result) {
  if (result == "success") {
    chrome.test.sendMessage("eject_ok");
  } else if (result == "in_use") {
    chrome.test.sendMessage("eject_in_use");
  } else if (result == "no_such_device") {
    chrome.test.sendMessage("eject_no_such_device");
  } else {
    chrome.test.sendMessage("eject_failure");
  }
};

function ejectTest() {
  chrome.mediaGalleriesPrivate.ejectDevice(attachedDeviceId, ejectCallback);
};

function addAttachListener() {
  chrome.mediaGalleriesPrivate.onDeviceAttached.addListener(testAttach);
  chrome.test.sendMessage('add_attach_ok');
};

function removeAttachListener() {
  chrome.mediaGalleriesPrivate.onDeviceAttached.removeListener(testAttach);
  chrome.test.sendMessage('remove_attach_ok');
};

function ejectFailTest() {
  chrome.mediaGalleriesPrivate.ejectDevice('-1', ejectCallback);
};
