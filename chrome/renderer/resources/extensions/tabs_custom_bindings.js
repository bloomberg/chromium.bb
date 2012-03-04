// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the tabs API.

(function() {

native function GetChromeHidden();
native function OpenChannelToTab();

var chromeHidden = GetChromeHidden();

chromeHidden.registerCustomHook('tabs', function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('connect', function(tabId, connectInfo) {
    var name = '';
    if (connectInfo) {
      name = connectInfo.name || name;
    }
    var portId = OpenChannelToTab(tabId, extensionId, name);
    return chromeHidden.Port.createPort(portId, name);
  });

  apiFunctions.setHandleRequest('sendRequest',
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
});

})();
