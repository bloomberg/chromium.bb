// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let getApi = requireNative('apiGetter').get;
let mojoPrivate = getApi('mojoPrivate');
let test = getApi('test');
let unittestBindings = require('test_environment_specific_bindings');

unittestBindings.exportTests([
  function testRequireAsync() {
    mojoPrivate.requireAsync('add').then(
        test.callbackPass(function(add) {
          test.assertEq('function', typeof add);
        }));
  },
], test.runTests, exports);
