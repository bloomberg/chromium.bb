// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the declarativeWebRequest API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var utils = require('utils');

chromeHidden.registerCustomHook('declarativeWebRequest', function(api) {
  // Returns the schema definition of type |typeId| defined in |namespace|.
  function getSchema(namespace, typeId) {
    var apiSchema = utils.lookup(api.apiDefinitions, 'namespace', namespace);
    var resultSchema = utils.lookup(
        apiSchema.types, 'id', namespace + '.' + typeId);
    return resultSchema;
  }

  // Helper function for the constructor of concrete datatypes of the
  // declarative webRequest API.
  // Makes sure that |this| contains the union of parameters and
  // {'instanceType': 'declarativeWebRequest.' + typeId} and validates the
  // generated union dictionary against the schema for |typeId|.
  function setupInstance(instance, parameters, typeId) {
    for (var key in parameters) {
      if (parameters.hasOwnProperty(key)) {
        instance[key] = parameters[key];
      }
    }
    instance.instanceType = 'declarativeWebRequest.' + typeId;
    var schema = getSchema('declarativeWebRequest', typeId);
    chromeHidden.validate([instance], [schema]);
  }

  // Setup all data types for the declarative webRequest API.
  chrome.declarativeWebRequest.RequestMatcher = function(parameters) {
    setupInstance(this, parameters, 'RequestMatcher');
  };
  chrome.declarativeWebRequest.CancelRequest = function(parameters) {
    setupInstance(this, parameters, 'CancelRequest');
  };
  chrome.declarativeWebRequest.RedirectRequest = function(parameters) {
    setupInstance(this, parameters, 'RedirectRequest');
  };
});
