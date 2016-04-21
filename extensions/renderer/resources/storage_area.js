// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var normalizeArgumentsAndValidate =
    require('schemaUtils').normalizeArgumentsAndValidate
var sendRequest = require('sendRequest').sendRequest;

function extendSchema(schema) {
  var extendedSchema = $Array.slice(schema);
  $Array.unshift(extendedSchema, {'type': 'string'});
  return extendedSchema;
}

function StorageArea(namespace, schema) {
  // Binds an API function for a namespace to its browser-side call, e.g.
  // storage.sync.get('foo') -> (binds to) ->
  // storage.get('sync', 'foo').
  //
  // TODO(kalman): Put as a method on CustombindingObject and re-use (or
  // even generate) for other APIs that need to do this. Same for other
  // callers of registerCustomType().
  var self = this;
  function bindApiFunction(functionName) {
    self[functionName] = function() {
      var funSchema = this.functionSchemas[functionName];
      var args = $Array.slice(arguments);
      args = normalizeArgumentsAndValidate(args, funSchema);
      return sendRequest(
          'storage.' + functionName,
          $Array.concat([namespace], args),
          extendSchema(funSchema.definition.parameters),
          {preserveNullInObjects: true});
    };
  }
  var apiFunctions = ['get', 'set', 'remove', 'clear', 'getBytesInUse'];
  $Array.forEach(apiFunctions, bindApiFunction);
}

exports.$set('StorageArea', StorageArea);
