// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the types API.

var binding = require('binding').Binding.create('types');

var chrome = requireNative('chrome').GetChrome();
var sendRequest = require('sendRequest').sendRequest;
var validate = require('schemaUtils').validate;

binding.registerCustomType('types.ChromeSetting', function() {

  function extendSchema(schema) {
    var extendedSchema = schema.slice();
    extendedSchema.unshift({'type': 'string'});
    return extendedSchema;
  }

  function ChromeSetting(prefKey, valueSchema) {
    this.get = function(details, callback) {
      var getSchema = this.functionSchemas.get.definition.parameters;
      validate([details, callback], getSchema);
      return sendRequest('types.ChromeSetting.get',
                         [prefKey, details, callback],
                         extendSchema(getSchema));
    };
    this.set = function(details, callback) {
      var setSchema = this.functionSchemas.set.definition.parameters.slice();
      setSchema[0].properties.value = valueSchema;
      validate([details, callback], setSchema);
      return sendRequest('types.ChromeSetting.set',
                         [prefKey, details, callback],
                         extendSchema(setSchema));
    };
    this.clear = function(details, callback) {
      var clearSchema = this.functionSchemas.clear.definition.parameters;
      validate([details, callback], clearSchema);
      return sendRequest('types.ChromeSetting.clear',
                         [prefKey, details, callback],
                         extendSchema(clearSchema));
    };
    this.onChange = new chrome.Event('types.ChromeSetting.' + prefKey +
                                     '.onChange');
  };

  return ChromeSetting;
});

exports.binding = binding.generate();
