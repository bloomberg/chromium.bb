// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the Serial API.
 *
 * The bindings are implemented by asynchronously delegating to the
 * serial_service module. The functions that apply to a particular connection
 * are delegated to the appropriate method on the Connection object specified by
 * the ID parameter.
 */

var binding = require('binding').Binding.create('serial');

function createAsyncProxy(targetPromise, functionNames) {
  var functionProxies = {};
  $Array.forEach(functionNames, function(name) {
    functionProxies[name] = function() {
      var args = arguments;
      return targetPromise.then(function(target) {
        return $Function.apply(target[name], target, args);
      });
    };
  });
  return functionProxies;
}

var serialService = createAsyncProxy(requireAsync('serial_service'), [
  'getDevices',
  'createConnection',
  'getConnection',
  'getConnections',
]);

function forwardToConnection(methodName) {
  return function(connectionId) {
    var args = $Array.slice(arguments, 1);
    return serialService.getConnection(connectionId).then(function(connection) {
      return $Function.apply(connection[methodName], connection, args);
    });
  };
}

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequestWithPromise('getDevices',
                                           serialService.getDevices);

  apiFunctions.setHandleRequestWithPromise('connect', function(path, options) {
    return serialService.createConnection(path, options).then(function(result) {
      return result.info;
    }).catch (function(e) {
      throw new Error('Failed to connect to the port.');
    });
  });

  apiFunctions.setHandleRequestWithPromise(
      'disconnect', forwardToConnection('close'));
  apiFunctions.setHandleRequestWithPromise(
      'getInfo', forwardToConnection('getInfo'));
  apiFunctions.setHandleRequestWithPromise(
      'update', forwardToConnection('setOptions'));
  apiFunctions.setHandleRequestWithPromise(
      'getControlSignals', forwardToConnection('getControlSignals'));
  apiFunctions.setHandleRequestWithPromise(
      'setControlSignals', forwardToConnection('setControlSignals'));
  apiFunctions.setHandleRequestWithPromise(
      'flush', forwardToConnection('flush'));
  apiFunctions.setHandleRequestWithPromise(
      'setPaused', forwardToConnection('setPaused'));

  apiFunctions.setHandleRequestWithPromise('getConnections', function() {
    return serialService.getConnections().then(function(connections) {
      var promises = [];
      for (var id in connections) {
        promises.push(connections[id].getInfo());
      }
      return Promise.all(promises);
    });
  });
});

exports.binding = binding.generate();
