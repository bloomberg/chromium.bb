// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.callbackFail;
var pass = chrome.test.callbackPass;

var PERMISSION_DENIED_ERROR = "Permission to access device denied";

var kUUID = "00001101-0000-1000-8000-00805f9b34fb";
var kAddress1 = "11:12:13:14:15:16";

chrome.test.runTests([
  function permissionRequest() {
    chrome.permissions.request(
        {
          permissions: [
            {"bluetoothDevices":[{"deviceAddress":kAddress1}]}
          ]
        },
        pass(function(granted) {
          // They were not granted, and there should be no error.
          assertTrue(granted);
          assertTrue(chrome.runtime.lastError === undefined);

          chrome.bluetooth.connect(
              {
                "deviceAddress":kAddress1,
                "serviceUuid":kUUID
              },
              pass(function(socket) {
                chrome.bluetooth.disconnect(
                    {"socketId":socket.id},
                    pass(function(){})
                );
              }));
        }));
  },
  function permissionDenied() {
    chrome.bluetooth.connect(
        {"deviceAddress":"55:44:33:22:11:00","serviceUuid":kUUID},
        fail(PERMISSION_DENIED_ERROR));
  }
]);
