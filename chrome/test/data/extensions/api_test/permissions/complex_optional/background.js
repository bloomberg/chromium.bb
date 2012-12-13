// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;
var listenOnce = chrome.test.listenOnce;
var pass = chrome.test.callbackPass;

function hasBluetoothDevice(address, permissions) {
  for (var i = 0; i < permissions.permissions.length; i += 1) {
    devices = permissions.permissions[i]["bluetoothDevices"];
    if (devices != undefined) {
      for (var j = 0; j < devices.length; j += 1) {
        if (devices[i]["deviceAddress"] == address)
          return true;
      }
    }
  }
  return false;
}

chrome.test.runTests([
  function request() {
    listenOnce(
        chrome.permissions.onAdded,
        function(permissions) {
          assertTrue(permissions.permissions.length == 1);
          assertTrue(hasBluetoothDevice('00:11:22:33:44:55', permissions));
        });
    chrome.permissions.request(
        { permissions:
            [{'bluetoothDevices':[{'deviceAddress':'00:11:22:33:44:55'}]}]
        },
        pass(function(granted) {
          assertTrue(granted);
          chrome.permissions.getAll(pass(
              function(permissions) {
                assertTrue(
                    hasBluetoothDevice('00:11:22:33:44:55', permissions));
              }));
        }));
  },
]);
