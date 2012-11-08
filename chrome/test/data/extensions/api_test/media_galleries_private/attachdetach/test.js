// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var attachedDeviceId = '42';

var testAttach = function(details) {
  // Save the device id for the detach test.
  attachedDeviceId = details.deviceId;
  // The C++ test code will check if this is ok.
  chrome.test.sendMessage('attach_test_ok,' + details.deviceName);
};

// Makes sure the detach device id matches and then ACK the detach event.
var testDetach = function(details) {
  // The C++ test does not know the device id, so check here.
  if (details.deviceId != attachedDeviceId) {
    chrome.test.fail('Bad device id in onDeviceDetached test.');
    return;
  }
  chrome.test.sendMessage('detach_test_ok');
};

// Simply ACK the detach event.
var testDummyDetach = function(details) {
  chrome.test.sendMessage('detach_test_ok');
};

// These functions add / remove listeners.
function addAttachListener() {
  chrome.mediaGalleriesPrivate.onDeviceAttached.addListener(testAttach);
  chrome.test.sendMessage('add_attach_ok');
}

function addDetachListener() {
  chrome.mediaGalleriesPrivate.onDeviceDetached.addListener(testDetach);
  chrome.test.sendMessage('add_detach_ok');
}

function addDummyDetachListener() {
  chrome.mediaGalleriesPrivate.onDeviceDetached.addListener(testDummyDetach);
  chrome.test.sendMessage('add_dummy_detach_ok');
}

function removeAttachListener() {
  chrome.mediaGalleriesPrivate.onDeviceAttached.removeListener(testAttach);
  chrome.test.sendMessage('remove_attach_ok');
}

function removeDummyDetachListener() {
  chrome.mediaGalleriesPrivate.onDeviceAttached.removeListener(testDummyDetach);
  chrome.test.sendMessage('remove_dummy_detach_ok');
}
