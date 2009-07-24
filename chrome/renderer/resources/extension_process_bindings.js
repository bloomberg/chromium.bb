// Copyright (c) 2009 The chrome Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function GetExtensionAPIDefinition();
  native function StartRequest();
  native function GetCurrentPageActions();
  native function GetViews();
  native function GetChromeHidden();
  native function GetNextRequestId();
  native function OpenChannelToTab();

  if (!chrome)
    chrome = {};

  var chromeHidden = GetChromeHidden();

  // Validate arguments.
  function validate(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chrome.JSONSchemaValidator();
        validator.validate(args[i], schemas[i]);
        if (validator.errors.length == 0)
          continue;

        var message = "Invalid value for argument " + i + ". ";
        for (var i = 0, err; err = validator.errors[i]; i++) {
          if (err.path) {
            message += "Property '" + err.path + "': ";
          }
          message += err.message;
          message = message.substring(0, message.length - 1);
          message += ", ";
        }
        message = message.substring(0, message.length - 2);
        message += ".";

        throw new Error(message);
      } else if (!schemas[i].optional) {
        throw new Error("Parameter " + i + " is required.");
      }
    }
  }

  // Callback handling.
  var callbacks = [];
  chromeHidden.handleResponse = function(requestId, name,
                                         success, response, error) {
    try {
      if (!success) {
        if (!error)
          error = "Unknown error."
        console.error("Error during " + name + ": " + error);
        return;
      }

      if (callbacks[requestId]) {
        if (response) {
          callbacks[requestId](JSON.parse(response));
        } else {
          callbacks[requestId]();
        }
      }
    } finally {
      delete callbacks[requestId];
    }
  };

  // Send an API request and optionally register a callback.
  function sendRequest(functionName, args, callback) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (args === undefined)
      args = null;
    var sargs = JSON.stringify(args);
    var requestId = GetNextRequestId();
    var hasCallback = false;
    if (callback) {
      hasCallback = true;
      callbacks[requestId] = callback;
    }
    StartRequest(functionName, sargs, requestId, hasCallback);
  }
  
  // Read api definitions and setup api functions in the chrome namespace.
  // TODO(rafaelw): Consider defining a json schema for an api definition
  //   and validating either here, in a unit_test or both.
  // TODO(rafaelw): Handle synchronous functions.
  // TOOD(rafaelw): Consider providing some convenient override points
  //   for api functions that wish to insert themselves into the call. 
  var apiDefinitions = JSON.parse(GetExtensionAPIDefinition());

  // Using forEach for convenience, and to bind |module|s & |apiDefs|s via
  // closures.
  function forEach(a, f) {
    for (var i = 0; i < a.length; i++) {
      f(a[i], i);
    }
  }

  forEach(apiDefinitions, function(apiDef) {
    var module = {};
    chrome[apiDef.namespace] = module;
    
    // Setup Functions.
    forEach(apiDef.functions, function(functionDef) {
      var paramSchemas = functionDef.parameters;
      
      module[functionDef.name] = function() {
        validate(arguments, paramSchemas);
        
        var functionName = apiDef.namespace + "." + functionDef.name;
        var args = null;
        var callback = null;
        var argCount = arguments.length;
        
        // Look for callback param.
        if (paramSchemas.length > 0 &&
            arguments.length == paramSchemas.length &&
            paramSchemas[paramSchemas.length - 1].type == "function") {
          callback = arguments[paramSchemas.length - 1];
          --argCount;
        }
        
        // Calls with one argument expect singular argument. Calls with multiple
        // expect a list.
        if (argCount == 1)
          args = arguments[0];
        if (argCount > 1) {
          args = [];
          for (var k = 0; k < argCount; k++) {
            args[k] = arguments[k];
          }
        }
        
        // Make the request.
        sendRequest(functionName, args, callback);        
      }
    });
    
    // Setup Events
    forEach(apiDef.events, function(eventDef) {
      var eventName = apiDef.namespace + "." + eventDef.name;
      module[eventDef.name] = new chrome.Event(eventName);
    });
  });

  // --- Setup additional api's not currently handled in common/extensions/api

  // Page action events send (pageActionId, {tabId, tabUrl}).
  function setupPageActionEvents(extensionId) {
    var pageActions = GetCurrentPageActions();
    var eventName = "";
    for (var i = 0; i < pageActions.length; ++i) {
      eventName = extensionId + "/" + pageActions[i];
      // Setup events for each extension_id/page_action_id string we find.
      chrome.pageActions[pageActions[i]] = new chrome.Event(eventName);
    }
  }

  // Tabs connect()
  chrome.tabs.connect = function(tabId, opt_name) {
    validate(arguments, arguments.callee.params);
    var portId = OpenChannelToTab(tabId, chrome.extension.id_, opt_name || "");
    return chromeHidden.Port.createPort(portId, opt_name);
  };

  chrome.tabs.connect.params = [
    {type: "integer", optional: true, minimum: 0},
    {type: "string", optional: true}
  ];

  // chrome.self / chrome.extension.
  chrome.self = chrome.self || {};

  chromeHidden.onLoad.addListener(function (extensionId) {
    chrome.extension = new chrome.Extension(extensionId);
    // TODO(mpcomplete): self.onConnect is deprecated.  Remove it at 1.0.
    // http://code.google.com/p/chromium/issues/detail?id=16356
    chrome.self.onConnect = chrome.extension.onConnect;

    setupPageActionEvents(extensionId);
  });

  chrome.self.getViews = function() {
    return GetViews();
  }
})();
