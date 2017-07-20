// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let getApi = requireNative('apiGetter').get;
let mojoPrivate = getApi('mojoPrivate');
let test = getApi('test');
let unittestBindings = require('test_environment_specific_bindings');

unittestBindings.exportTests([
  function testDefine() {
    mojoPrivate.define('testModule', [
      'mojo/public/js/codec',
    ], test.callbackPass(function(codec) {
      test.assertEq('function', typeof codec.Message);
    }));
  },

  function testDefineRegistersModule() {
    mojoPrivate.define('testModule', ['dependency'],
                       test.callbackPass(function(module) {
      test.assertEq(12345, module.result);
    }));
    mojoPrivate.define('dependency', test.callbackPass(function() {
      return {result: 12345};
    }));
  },

  function testDefineModuleDoesNotExist() {
    mojoPrivate.define('testModule', ['does not exist!'], test.fail);
    test.succeed();
  },

  function testRequireAsync() {
    mojoPrivate.requireAsync('mojo/public/js/codec').then(
        test.callbackPass(function(codec) {
          test.assertEq('function', typeof codec.Message);
        }));
  },

  function testDefineAndRequire() {
    mojoPrivate.define('testModule', ['dependency'],
                       test.callbackPass(function(module) {
      test.assertEq(12345, module.result);
    }));
    mojoPrivate.define('dependency', test.callbackPass(function() {
      return {result: 12345};
    }));
    mojoPrivate.requireAsync('dependency').then(
        test.callbackPass(),
        test.fail);
  }
], test.runTests, exports);
