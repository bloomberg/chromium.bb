// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental.storage API.

(function() {

native function GetChromeHidden();

var chromeHidden = GetChromeHidden();

chromeHidden.registerCustomType('StorageNamespace',
    function(typesAPI) {
  var sendRequest = typesAPI.sendRequest;

  function extendSchema(schema) {
    var extendedSchema = schema.slice();
    extendedSchema.unshift({'type': 'string'});
    return extendedSchema;
  }

  function StorageNamespace(namespace, schema) {
    // Binds an API function for a namespace to its browser-side call, e.g.
    // experimental.storage.sync.get('foo') -> (binds to) ->
    // experimental.storage.get('sync', 'foo').
    //
    // TODO(kalman): Put as a method on CustomBindingsObject and re-use (or
    // even generate) for other APIs that need to do this. Same for other
    // callers of registerCustomType().
    function bindApiFunction(functionName) {
      this[functionName] = function() {
        var schema = this.parameters[functionName];
        chromeHidden.validate(arguments, schema);
        return sendRequest(
            'experimental.storage.' + functionName,
            [namespace].concat(Array.prototype.slice.call(arguments)),
            extendSchema(schema));
      };
    }
    ['get', 'set', 'remove', 'clear'].forEach(bindApiFunction.bind(this));
  }

  return StorageNamespace;
});

})();
