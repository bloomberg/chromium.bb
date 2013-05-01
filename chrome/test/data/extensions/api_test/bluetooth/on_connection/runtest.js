// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceName;
var deviceAddress;
var profileUuid;

function testOnConnectionEvent() {
  chrome.test.assertEq('d1', deviceName);
  chrome.test.assertEq('11:12:13:14:15:16', deviceAddress);
  chrome.test.assertEq('1234', profileUuid);

  chrome.test.succeed();
}

chrome.bluetooth.onConnection.addListener(
  function(socket) {
    deviceName = socket.device.name;
    deviceAddress = socket.device.address;
    profileUuid = socket.profile.uuid;
    chrome.bluetooth.disconnect({'socket': socket});
  });

chrome.test.sendMessage('ready',
  function(message) {
    chrome.test.runTests([testOnConnectionEvent]);
  });
