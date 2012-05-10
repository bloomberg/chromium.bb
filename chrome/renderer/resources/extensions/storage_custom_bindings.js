// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the storage API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;

chromeHidden.registerCustomType('StorageArea', function() {
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
    function bindApiFunction(functionName) {
      this[functionName] = function() {
        var schema = this.parameters[functionName];
        chromeHidden.validate(arguments, schema);
        return sendRequest(
            'storage.' + functionName,
            [namespace].concat(Array.prototype.slice.call(arguments)),
            extendSchema(schema));
      };
    }
    var apiFunctions = ['get', 'set', 'remove', 'clear', 'getBytesInUse'];
    apiFunctions.forEach(bindApiFunction.bind(this));
  }

  return StorageArea;
});
