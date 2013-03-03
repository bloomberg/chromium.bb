// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the storage API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var normalizeArgumentsAndValidate =
    require('schemaUtils').normalizeArgumentsAndValidate
var sendRequest = require('sendRequest').sendRequest;

chromeHidden.registerCustomType('storage.StorageArea', function() {
  function extendSchema(schema) {
    var extendedSchema = schema.slice();
    extendedSchema.unshift({'type': 'string'});
    return extendedSchema;
  }

  function StorageArea(namespace, schema) {
    // Binds an API function for a namespace to its browser-side call, e.g.
    // storage.sync.get('foo') -> (binds to) ->
    // storage.get('sync', 'foo').
    //
    // TODO(kalman): Put as a method on CustomBindingsObject and re-use (or
    // even generate) for other APIs that need to do this. Same for other
    // callers of registerCustomType().
    var self = this;
    function bindApiFunction(functionName) {
      self[functionName] = function() {
        var funSchema = this.functionSchemas[functionName];
        var args = Array.prototype.slice.call(arguments);
        args = normalizeArgumentsAndValidate(args, funSchema);
        return sendRequest(
            'storage.' + functionName,
            [namespace].concat(args),
            extendSchema(funSchema.definition.parameters),
            {preserveNullInObjects: true});
      };
    }
    var apiFunctions = ['get', 'set', 'remove', 'clear', 'getBytesInUse'];
    apiFunctions.forEach(bindApiFunction);
  }

  return StorageArea;
});
