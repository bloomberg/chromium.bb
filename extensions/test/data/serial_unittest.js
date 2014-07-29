// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var test = require('test').binding;
var serial = require('serial').binding;
var unittestBindings = require('test_environment_specific_bindings');

unittestBindings.exportTests([
  function testGetDevices() {
    serial.getDevices(test.callbackPass(function(devices) {
      test.assertEq(3, devices.length);
      test.assertEq(4, $Object.keys(devices[0]).length);
      test.assertEq('device', devices[0].path);
      test.assertEq(1234, devices[0].vendorId);
      test.assertEq(5678, devices[0].productId);
      test.assertEq('foo', devices[0].displayName);
      test.assertEq(1, $Object.keys(devices[1]).length);
      test.assertEq('another_device', devices[1].path);
      test.assertEq(1, $Object.keys(devices[2]).length);
      test.assertEq('', devices[2].path);
    }));
  },
], test.runTests, exports);
