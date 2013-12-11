// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SendResponse = requireNative('pepper_request_natives').SendResponse;
var GetAvailability = requireNative('v8_context').GetAvailability;
var utils = require('utils');
var schemaRegistry = requireNative('schema_registry');

function takesCallback(targetName) {
  var parts = $String.split(targetName, '.');
  var schemaName = $Array.join($Array.slice(parts, 0, parts.length - 1), '.');
  var functionName = parts[parts.length - 1];
  var functions = schemaRegistry.GetSchema(schemaName).functions;
  var parameters = utils.lookup(functions, 'name', functionName).parameters;
  return parameters.length > 0 &&
         parameters[parameters.length - 1].type == 'function';
}

function resolveName(name) {
  var availability = GetAvailability(name);
  if (!availability.is_available)
    throw Error(availability.message);
  var item = chrome;
  var nameComponents = $String.split(name, '.');
  for (var i = 0; i < nameComponents.length; i++) {
    item = item[nameComponents[i]];
  }
  return item;
}

function startRequest(targetName, requestId) {
  var args = $Array.slice(arguments, 2);
  try {
    var hasCallback = takesCallback(targetName);
    if (hasCallback) {
      args.push(function() {
        var error = null;
        if (chrome.runtime.lastError)
          error = chrome.runtime.lastError.message;
        SendResponse(requestId, $Array.slice(arguments), error);
      });
    }
    var target = resolveName(targetName);
    var result = $Function.apply(target, null, args);
    if (!hasCallback)
      SendResponse(requestId, [result], null);
  } catch (e) {
    // TODO(sammc): Catch this from C++.
    return e.message;
  }
}

exports.startRequest = startRequest;
