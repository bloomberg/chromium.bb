// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Must be packed to ../enterprise_device_attributes.crx using the private key
// ../enterprise_device_attributes.pem .

// The expected directory device id, that is passed by caller.
var expected_value = location.search.substring(1);

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var callbackPass = chrome.test.callbackPass
var succeed = chrome.test.succeed;

assertTrue(chrome.enterprise.deviceAttributes.getDirectoryDeviceId(
    callbackPass(function(device_id) {
      assertEq(expected_value, device_id);
    })));

succeed();
