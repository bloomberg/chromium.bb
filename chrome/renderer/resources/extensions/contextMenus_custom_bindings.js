// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the contextMenus API.

(function() {

native function GetChromeHidden();
native function GetNextContextMenuId();

var chromeHidden = GetChromeHidden();
var apiFunctions = chromeHidden.schemaGeneratedBindings.apiFunctions;
var sendRequest = chromeHidden.schemaGeneratedBindings.sendRequest;

chromeHidden.onLoad.addListener(
    function(extensionId, isExtensionProcess, isIncognitoProcess) {
  chromeHidden.contextMenus = {};
  chromeHidden.contextMenus.handlers = {};
  var eventName = "contextMenus";
  chromeHidden.contextMenus.event = new chrome.Event(eventName);
  chromeHidden.contextMenus.ensureListenerSetup = function() {
    if (chromeHidden.contextMenus.listening) {
      return;
    }
    chromeHidden.contextMenus.listening = true;
    chromeHidden.contextMenus.event.addListener(function() {
      // An extension context menu item has been clicked on - fire the onclick
      // if there is one.
      var id = arguments[0].menuItemId;
      var onclick = chromeHidden.contextMenus.handlers[id];
      if (onclick) {
        onclick.apply(null, arguments);
      }
    });
  };

  apiFunctions.setHandleRequest("contextMenus.create", function() {
    var args = arguments;
    var id = GetNextContextMenuId();
    args[0].generatedId = id;
    sendRequest(this.name,
                args,
                this.definition.parameters,
                {customCallback: this.customCallback});
    return id;
  });

  apiFunctions.setCustomCallback("contextMenus.create",
      function(name, request, response) {
    if (chrome.extension.lastError) {
      return;
    }

    var id = request.args[0].generatedId;

    // Set up the onclick handler if we were passed one in the request.
    var onclick = request.args.length ? request.args[0].onclick : null;
    if (onclick) {
      chromeHidden.contextMenus.ensureListenerSetup();
      chromeHidden.contextMenus.handlers[id] = onclick;
    }
  });

  apiFunctions.setCustomCallback("contextMenus.remove",
      function(name, request, response) {
    if (chrome.extension.lastError) {
      return;
    }
    var id = request.args[0];
    delete chromeHidden.contextMenus.handlers[id];
  });

  apiFunctions.setCustomCallback("contextMenus.update",
      function(name, request, response) {
    if (chrome.extension.lastError) {
      return;
    }
    var id = request.args[0];
    if (request.args[1].onclick) {
      chromeHidden.contextMenus.handlers[id] = request.args[1].onclick;
    }
  });

  apiFunctions.setCustomCallback("contextMenus.removeAll",
      function(name, request, response) {
    if (chrome.extension.lastError) {
      return;
    }
    chromeHidden.contextMenus.handlers = {};
  });
});

})();
