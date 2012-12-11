// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Array of each device list update.
var devices = [];

chrome.test.sendMessage('ready', function(message) {
  if (message == 'setup') {
    function handleOnDeviceList(deviceList) {
      var newDevices = [];
      for (var i = 0; i < deviceList.length; ++i) {
        newDevices.push(deviceList[i]);
      }
      devices.push(newDevices);
    }
    chrome.dial.onDeviceList.addListener(handleOnDeviceList);

    chrome.test.sendMessage('setup', function(message) {
      if (message == 'go') {
        chrome.test.runTests([
          function testEvents() {
            // Make sure there were 3 devices.
            chrome.test.assertEq(3, devices.length);

            // Make sure each update contained 1 more device than the previous.
            for (var i = 0; i < devices.length; ++i) {
              chrome.test.assertEq(i + 1, devices[i].length);
            }

            // Just check the first device in the first update and the last
            // device in the last update.
            // Not exposing the device id right now.
            chrome.test.assertTrue(!('deviceId' in devices[0][0]));
            chrome.test.assertEq("1", devices[0][0].deviceLabel);
            chrome.test.assertEq("http://1",
                                 devices[0][0].deviceDescriptionUrl);

            chrome.test.assertTrue(!('deviceId' in devices[2][2]));
            chrome.test.assertEq("3", devices[2][2].deviceLabel);
            chrome.test.assertEq("http://3",
                                 devices[2][2].deviceDescriptionUrl);

            chrome.test.succeed();
          }
        ]);
      }
    });
  }
});
