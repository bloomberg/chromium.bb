// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevices() {
  chrome.test.assertEq(2, devices['all'].length);
  chrome.test.assertEq('d1', devices['all'][0].name);
  chrome.test.assertEq('d2', devices['all'][1].name);

  chrome.test.assertEq(1, devices['name'].length);
  chrome.test.assertEq('d1', devices['name'][0].name);

  chrome.test.assertEq(1, devices['uuid'].length);
  chrome.test.assertEq('d2', devices['uuid'][0].name);

  chrome.test.succeed();
}

var devices = {
  'all': [],
  'name': [],
  'uuid': []
};
function recordDevicesInto(arrayKey) {
  return function(device) {
    devices[arrayKey].push(device);
  };
}

chrome.experimental.bluetooth.getDevices(
    {
      deviceCallback:recordDevicesInto('all')
    },
    function() {
      chrome.experimental.bluetooth.getDevices(
          {
            name:'fooservice',
            deviceCallback:recordDevicesInto('name')
          },
          function() {
            chrome.experimental.bluetooth.getDevices(
                {
                  uuid:'00000010-0000-1000-8000-00805f9b34fb',
                  deviceCallback:recordDevicesInto('uuid')
                },
                function() {
                  chrome.test.sendMessage('ready',
                      function(message) {
                        chrome.test.runTests([
                            testGetDevices
                        ]);
                    });
                });
          });
    });
