// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Unit tests for the keep-alive client.
 *
 * They are launched by extensions/renderer/mojo/keep_alive_client_unittest.cc.
 */

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');
var utils = require('utils');

var shouldSucceed;

// We need to set custom bindings for a real API and serial.getDevices has a
// simple signature.
var binding = require('binding').Binding.create('serial');
binding.registerCustomHook(function(bindingsAPI) {
  bindingsAPI.apiFunctions.setHandleRequestWithPromise('getDevices',
      function() {
    if (shouldSucceed)
      return Promise.resolve([]);
    else
      return Promise.reject();
  });
});
var apiFunction = binding.generate().getDevices;

unittestBindings.exportTests([
  // Test that a keep alive is created and destroyed for a successful API call.
  function testKeepAliveWithSuccessfulCall() {
    shouldSucceed = true;
    utils.promise(apiFunction).then(test.succeed, test.fail);
  },

  // Test that a keep alive is created and destroyed for an unsuccessful API
  // call.
  function testKeepAliveWithError() {
    shouldSucceed = false;
    utils.promise(apiFunction).then(test.fail, test.succeed);
  },
], test.runTests, exports);
