// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = require('binding').Binding.create('serial');

function createAsyncProxy(targetPromise, functionNames) {
  var functionProxies = {};
  $Array.forEach(functionNames, function(name) {
    functionProxies[name] = function() {
      var args = arguments;
      return targetPromise.then(function(target) {
        return $Function.apply(target[name], target, args);
      });
    }
  });
  return functionProxies;
}

var serialService = createAsyncProxy(requireAsync('serial_service'), [
  'getDevices',
]);

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequestWithPromise('getDevices',
                                           serialService.getDevices);
});

exports.binding = binding.generate();
