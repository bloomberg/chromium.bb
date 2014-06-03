// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the contextMenus API.

var binding = require('binding').Binding.create('contextMenus');

var contextMenuNatives = requireNative('context_menus');
var sendRequest = require('sendRequest').sendRequest;
var Event = require('event_bindings').Event;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var contextMenus = {};
  contextMenus.generatedIdHandlers = {};
  contextMenus.stringIdHandlers = {};
  var eventName = 'contextMenus';
  contextMenus.event = new Event(eventName);
  contextMenus.getIdFromCreateProperties = function(prop) {
    if (typeof(prop.id) !== 'undefined')
      return prop.id;
    return prop.generatedId;
  };
  contextMenus.handlersForId = function(id) {
    if (typeof(id) === 'number')
      return contextMenus.generatedIdHandlers;
    return contextMenus.stringIdHandlers;
  };
  contextMenus.ensureListenerSetup = function() {
    if (contextMenus.listening) {
      return;
    }
    contextMenus.listening = true;
    contextMenus.event.addListener(function() {
      // An extension context menu item has been clicked on - fire the onclick
      // if there is one.
      var id = arguments[0].menuItemId;
      var onclick = contextMenus.handlersForId(id)[id];
      if (onclick) {
        $Function.apply(onclick, null, arguments);
      }
    });
  };

  apiFunctions.setHandleRequest('create', function() {
    var args = arguments;
    var id = contextMenuNatives.GetNextContextMenuId();
    args[0].generatedId = id;
    var optArgs = {
      customCallback: this.customCallback,
    };
    sendRequest(this.name, args, this.definition.parameters, optArgs);
    return contextMenus.getIdFromCreateProperties(args[0]);
  });

  apiFunctions.setCustomCallback('create', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }

    var id = contextMenus.getIdFromCreateProperties(request.args[0]);

    // Set up the onclick handler if we were passed one in the request.
    var onclick = request.args.length ? request.args[0].onclick : null;
    if (onclick) {
      contextMenus.ensureListenerSetup();
      contextMenus.handlersForId(id)[id] = onclick;
    }
  });

  apiFunctions.setCustomCallback('remove', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    var id = request.args[0];
    delete contextMenus.handlersForId(id)[id];
  });

  apiFunctions.setCustomCallback('update', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    var id = request.args[0];
    if (request.args[1].onclick) {
      contextMenus.handlersForId(id)[id] = request.args[1].onclick;
    }
  });

  apiFunctions.setCustomCallback('removeAll',
                                 function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    contextMenus.generatedIdHandlers = {};
    contextMenus.stringIdHandlers = {};
  });
});

exports.binding = binding.generate();
