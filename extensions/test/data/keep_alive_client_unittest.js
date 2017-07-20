// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Unit tests for the keep-alive client.
 *
 * They are launched by extensions/renderer/mojo/keep_alive_client_unittest.cc.
 */

var getApi = requireNative('apiGetter').get;
var test = getApi('test');
var unittestBindings = require('test_environment_specific_bindings');
var utils = require('utils');

// We need to set custom bindings for a real API and serial.getDevices has a
// simple signature.
var serial = getApi('serial');
var apiFunction = serial.getDevices;

function runFunction() {
  apiFunction(function() {
    if (serial.shouldSucceed)
      test.assertNoLastError();
    else
      test.assertTrue(!!chrome.runtime.lastError);
    test.succeed();
  });
}

unittestBindings.exportTests([
  // Test that a keep alive is created and destroyed for a successful API call.
  function testKeepAliveWithSuccessfulCall() {
    // We cheat and put a property on the API object as a way of passing it to
    // the custom hooks.
    serial.shouldSucceed = true;
    runFunction();
  },

  // Test that a keep alive is created and destroyed for an unsuccessful API
  // call.
  function testKeepAliveWithError() {
    serial.shouldSucceed = true;
    runFunction();
  },
], test.runTests, exports);
