// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the types API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;

chromeHidden.registerCustomType('ChromeSetting', function() {

  function extendSchema(schema) {
    var extendedSchema = schema.slice();
    extendedSchema.unshift({'type': 'string'});
    return extendedSchema;
  }

  function ChromeSetting(prefKey, valueSchema) {
    this.get = function(details, callback) {
      var getSchema = this.parameters.get;
      chromeHidden.validate([details, callback], getSchema);
      return sendRequest('types.ChromeSetting.get',
                         [prefKey, details, callback],
                         extendSchema(getSchema));
    };
    this.set = function(details, callback) {
      var setSchema = this.parameters.set.slice();
      setSchema[0].properties.value = valueSchema;
      chromeHidden.validate([details, callback], setSchema);
      return sendRequest('types.ChromeSetting.set',
                         [prefKey, details, callback],
                         extendSchema(setSchema));
    };
    this.clear = function(details, callback) {
      var clearSchema = this.parameters.clear;
      chromeHidden.validate([details, callback], clearSchema);
      return sendRequest('types.ChromeSetting.clear',
                         [prefKey, details, callback],
                         extendSchema(clearSchema));
    };
    this.onChange = new chrome.Event('types.ChromeSetting.' + prefKey +
                                     '.onChange');
  };

  return ChromeSetting;
});
