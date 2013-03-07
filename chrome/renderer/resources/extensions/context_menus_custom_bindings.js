// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the contextMenus API.

var contextMenus = requireNative('context_menus');
var GetNextContextMenuId = contextMenus.GetNextContextMenuId;
var sendRequest = require('sendRequest').sendRequest;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('contextMenus', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  chromeHidden.contextMenus = {};
  chromeHidden.contextMenus.generatedIdHandlers = {};
  chromeHidden.contextMenus.stringIdHandlers = {};
  var eventName = 'contextMenus';
  chromeHidden.contextMenus.event = new chrome.Event(eventName);
  chromeHidden.contextMenus.getIdFromCreateProperties = function(prop) {
    if (typeof(prop.id) !== 'undefined')
      return prop.id;
    return prop.generatedId;
  };
  chromeHidden.contextMenus.handlersForId = function(id) {
    if (typeof(id) === 'number')
      return chromeHidden.contextMenus.generatedIdHandlers;
    return chromeHidden.contextMenus.stringIdHandlers;
  };
  chromeHidden.contextMenus.ensureListenerSetup = function() {
    if (chromeHidden.contextMenus.listening) {
      return;
    }
    chromeHidden.contextMenus.listening = true;
    chromeHidden.contextMenus.event.addListener(function() {
      // An extension context menu item has been clicked on - fire the onclick
      // if there is one.
      var id = arguments[0].menuItemId;
      var onclick = chromeHidden.contextMenus.handlersForId(id)[id];
      if (onclick) {
        onclick.apply(null, arguments);
      }
    });
  };

  apiFunctions.setHandleRequest('create', function() {
    var args = arguments;
    var id = GetNextContextMenuId();
    args[0].generatedId = id;
    var optArgs = {
      customCallback: this.customCallback,
    };
    sendRequest(this.name, args, this.definition.parameters, optArgs);
    return chromeHidden.contextMenus.getIdFromCreateProperties(args[0]);
  });

  apiFunctions.setCustomCallback('create', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }

    var id = chromeHidden.contextMenus.getIdFromCreateProperties(
        request.args[0]);

    // Set up the onclick handler if we were passed one in the request.
    var onclick = request.args.length ? request.args[0].onclick : null;
    if (onclick) {
      chromeHidden.contextMenus.ensureListenerSetup();
      chromeHidden.contextMenus.handlersForId(id)[id] = onclick;
    }
  });

  apiFunctions.setCustomCallback('remove', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    var id = request.args[0];
    delete chromeHidden.contextMenus.handlersForId(id)[id];
  });

  apiFunctions.setCustomCallback('update', function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    var id = request.args[0];
    if (request.args[1].onclick) {
      chromeHidden.contextMenus.handlersForId(id)[id] = request.args[1].onclick;
    }
  });

  apiFunctions.setCustomCallback('removeAll',
                                 function(name, request, response) {
    if (chrome.runtime.lastError) {
      return;
    }
    chromeHidden.contextMenus.generatedIdHandlers = {};
    chromeHidden.contextMenus.stringIdHandlers = {};
  });
});
