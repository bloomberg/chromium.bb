// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the tabs API.

(function() {

native function GetChromeHidden();
native function OpenChannelToTab();

var chromeHidden = GetChromeHidden();

chromeHidden.registerCustomHook('tabs', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('tabs.connect', function(tabId, connectInfo) {
    var name = '';
    if (connectInfo) {
      name = connectInfo.name || name;
    }
    var portId = OpenChannelToTab(tabId, chromeHidden.extensionId, name);
    return chromeHidden.Port.createPort(portId, name);
  });

  apiFunctions.setHandleRequest('tabs.sendRequest',
      function(tabId, request, responseCallback) {
    var port = chrome.tabs.connect(tabId,
                                   {name: chromeHidden.kRequestChannel});
    port.postMessage(request);
    port.onDisconnect.addListener(function() {
      // For onDisconnects, we only notify the callback if there was an error.
      if (chrome.extension.lastError && responseCallback)
        responseCallback();
    });
    port.onMessage.addListener(function(response) {
      try {
        if (responseCallback)
          responseCallback(response);
      } finally {
        port.disconnect();
        port = null;
      }
    });
  });

  // TODO(skerner,mtytel): The next step to omitting optional arguments is the
  // replacement of this code with code that matches arguments by type.
  // Once this is working for captureVisibleTab() it can be enabled for
  // the rest of the API. See crbug/29215 .
  apiFunctions.setUpdateArgumentsPreValidate('tabs.captureVisibleTab',
                                             function() {
    // Old signature:
    //    captureVisibleTab(int windowId, function callback);
    // New signature:
    //    captureVisibleTab(int windowId, object details, function callback);
    var newArgs;
    if (arguments.length == 2 && typeof(arguments[1]) == 'function') {
      // If the old signature is used, add a null details object.
      newArgs = [arguments[0], null, arguments[1]];
    } else {
      newArgs = arguments;
    }
    return newArgs;
  });
});

})();
