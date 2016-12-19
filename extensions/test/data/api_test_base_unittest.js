// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');

unittestBindings.exportTests([
  function testEnvironment() {
    test.assertTrue(!!$Array);
    test.assertTrue(!!$Function);
    test.assertTrue(!!$JSON);
    test.assertTrue(!!$Object);
    test.assertTrue(!!$RegExp);
    test.assertTrue(!!$String);
    test.assertTrue(!!privates);
    test.assertTrue(!!define);
    test.assertTrue(!!require);
    test.assertTrue(!!requireNative);
    test.assertTrue(!!requireAsync);
    test.assertEq(undefined, chrome.runtime.lastError);
    test.assertEq(undefined, chrome.extension.lastError);
    test.succeed();
  },
  function testPromisesRun() {
    Promise.resolve().then(test.callbackPass());
  },
  function testCommonModulesAreAvailable() {
    var binding = require('binding');
    var sendRequest = require('sendRequest');
    var lastError = require('lastError');
    test.assertTrue(!!binding);
    test.assertTrue(!!sendRequest);
    test.assertTrue(!!lastError);
    test.succeed();
  },
  function testMojoModulesAreAvailable() {
    Promise.all([
      requireAsync('mojo/public/js/bindings'),
      requireAsync('mojo/public/js/core'),
      requireAsync('content/public/renderer/frame_interfaces'),
    ]).then(test.callback(function(modules) {
      var bindings = modules[0];
      var core = modules[1];
      var frameInterfaces = modules[2];
      test.assertTrue(!!bindings.Binding);
      test.assertTrue(!!core.createMessagePipe);
      test.assertTrue(!!frameInterfaces.getInterface);
    }));
  },
  function testTestBindings() {
    var counter = 0;
    function increment() {
      counter++;
    }
    test.runWithUserGesture(increment);
    test.runWithoutUserGesture(increment);
    test.assertEq(2, counter);
    test.assertFalse(test.isProcessingUserGesture());
    test.assertTrue(!!test.getApiFeatures());
    test.assertEq(0, test.getApiDefinitions().length);
    test.succeed();
  }
], test.runTests, exports);
