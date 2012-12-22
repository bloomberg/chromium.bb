// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the declarativeWebRequest API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var utils = require('utils');
var validate = require('schemaUtils').validate;

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
    validate([instance], [schema]);
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
  chrome.declarativeWebRequest.SetRequestHeader = function(parameters) {
    setupInstance(this, parameters, 'SetRequestHeader');
  };
  chrome.declarativeWebRequest.RemoveRequestHeader = function(parameters) {
    setupInstance(this, parameters, 'RemoveRequestHeader');
  };
  chrome.declarativeWebRequest.AddResponseHeader = function(parameters) {
    setupInstance(this, parameters, 'AddResponseHeader');
  };
  chrome.declarativeWebRequest.RemoveResponseHeader = function(parameters) {
    setupInstance(this, parameters, 'RemoveResponseHeader');
  };
  chrome.declarativeWebRequest.RedirectToTransparentImage =
      function(parameters) {
    setupInstance(this, parameters, 'RedirectToTransparentImage');
  };
  chrome.declarativeWebRequest.RedirectToEmptyDocument = function(parameters) {
    setupInstance(this, parameters, 'RedirectToEmptyDocument');
  };
  chrome.declarativeWebRequest.RedirectByRegEx = function(parameters) {
    setupInstance(this, parameters, 'RedirectByRegEx');
  };
  chrome.declarativeWebRequest.IgnoreRules = function(parameters) {
    setupInstance(this, parameters, 'IgnoreRules');
  };
  chrome.declarativeWebRequest.AddRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'AddRequestCookie');
  };
  chrome.declarativeWebRequest.AddResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'AddResponseCookie');
  };
  chrome.declarativeWebRequest.EditRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'EditRequestCookie');
  };
  chrome.declarativeWebRequest.EditResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'EditResponseCookie');
  };
  chrome.declarativeWebRequest.RemoveRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'RemoveRequestCookie');
  };
  chrome.declarativeWebRequest.RemoveResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'RemoveResponseCookie');
  };
  chrome.declarativeWebRequest.SendMessageToExtension = function(parameters) {
    setupInstance(this, parameters, 'SendMessageToExtension');
  };
});
