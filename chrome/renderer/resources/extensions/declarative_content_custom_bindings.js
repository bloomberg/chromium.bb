// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the declarativeContent API.

var binding = require('binding').Binding.create('declarativeContent');

var utils = require('utils');
var validate = require('schemaUtils').validate;

binding.registerCustomHook( function(api) {
  var declarativeContent = api.compiledApi;

  // Returns the schema definition of type |typeId| defined in |namespace|.
  function getSchema(typeId) {
    return utils.lookup(api.schema.types,
                        'id',
                        'declarativeContent.' + typeId);
  }

  // Helper function for the constructor of concrete datatypes of the
  // declarative content API.
  // Makes sure that |this| contains the union of parameters and
  // {'instanceType': 'declarativeContent.' + typeId} and validates the
  // generated union dictionary against the schema for |typeId|.
  function setupInstance(instance, parameters, typeId) {
    for (var key in parameters) {
      if (parameters.hasOwnProperty(key)) {
        instance[key] = parameters[key];
      }
    }
    instance.instanceType = 'declarativeContent.' + typeId;
    var schema = getSchema(typeId);
    validate([instance], [schema]);
  }

  // Setup all data types for the declarative content API.
  declarativeContent.PageStateMatcher = function(parameters) {
    setupInstance(this, parameters, 'PageStateMatcher');
  };
  declarativeContent.ShowPageAction = function(parameters) {
    setupInstance(this, parameters, 'ShowPageAction');
  };
});

exports.binding = binding.generate();
