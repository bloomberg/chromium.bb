// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $ = document.getElementById.bind(document);

function checkOneDevice() {
  var devices = $('register-device-list').children;
  assertEquals(1, devices.length);
  var firstDevice = devices[0];

  var deviceName = firstDevice.querySelector('.device-name').textContent;
  assertEquals('Sample device', deviceName);

  var deviceDescription =
        firstDevice.querySelector('.device-subline').textContent;
  assertEquals('Sample device description', deviceDescription);

  var button = firstDevice.querySelector('button');
  // Button should be disabled since there is no logged in user.
  assertEquals(true, button.disabled);
}

function checkNoDevices() {
  assertEquals(0, $('register-device-list').children.length);
}
