// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the extension API.

(function() {

native function GetChromeHidden();
native function GetExtensionViews();
native function OpenChannelToExtension(sourceId, targetId, name);

var chromeHidden = GetChromeHidden();

// This should match chrome.windows.WINDOW_ID_NONE.
//
// We can't use chrome.windows.WINDOW_ID_NONE directly because the
// chrome.windows API won't exist unless this extension has permission for it;
// which may not be the case.
var WINDOW_ID_NONE = -1;

chromeHidden.registerCustomHook('extension',
                                function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getViews', function(properties) {
    var windowId = WINDOW_ID_NONE;
    var type = 'ALL';
    if (properties) {
      if (properties.type != null) {
        type = properties.type;
      }
      if (properties.windowId != null) {
        windowId = properties.windowId;
      }
    }
    return GetExtensionViews(windowId, type) || null;
  });

  apiFunctions.setHandleRequest('getBackgroundPage', function() {
    return GetExtensionViews(-1, 'BACKGROUND')[0] || null;
  });

  apiFunctions.setHandleRequest('getExtensionTabs', function(windowId) {
    if (windowId == null)
      windowId = WINDOW_ID_NONE;
    return GetExtensionViews(windowId, 'TAB');
  });

  apiFunctions.setHandleRequest('getURL', function(path) {
    path = String(path);
    if (!path.length || path[0] != '/')
      path = '/' + path;
    return 'chrome-extension://' + extensionId + path;
  });

  apiFunctions.setUpdateArgumentsPreValidate('sendRequest', function() {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   extension.sendRequest(req)     -> extension.sendRequest(null, req)
    //   extension.sendRequest(req, cb) -> extension.sendRequest(null, req, cb)
    var lastArg = arguments.length - 1;

    // responseCallback (last argument) is optional.
    var responseCallback = null;
    if (typeof(arguments[lastArg]) == 'function')
      responseCallback = arguments[lastArg--];

    // request (second argument) is required.
    var request = arguments[lastArg--];

    // targetId (first argument, extensionId in the manfiest) is optional.
    var targetId = null;
    if (lastArg >= 0)
      targetId = arguments[lastArg--];

    if (lastArg != -1)
      throw new Error('Invalid arguments to sendRequest.');
    return [targetId, request, responseCallback];
  });

  apiFunctions.setHandleRequest('sendRequest',
                                function(targetId, request, responseCallback) {
    if (!targetId)
      targetId = extensionId;
    if (!responseCallback)
      responseCallback = function() {};

    var connectInfo = { name: chromeHidden.kRequestChannel };
    var port = chrome.extension.connect(targetId, connectInfo);

    port.postMessage(request);
    port.onDisconnect.addListener(function() {
      // For onDisconnects, we only notify the callback if there was an error
      try {
        if (chrome.extension.lastError)
          responseCallback();
      } finally {
        port = null;
      }
    });
    port.onMessage.addListener(function(response) {
      try {
        responseCallback(response);
      } finally {
        port.disconnect();
        port = null;
      }
    });
  });

  apiFunctions.setUpdateArgumentsPreValidate('connect', function() {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   extension.connect()   -> extension.connect(null, null)
    //   extension.connect({}) -> extension.connect(null, {})
    var nextArg = 0;

    // targetId (first argument) is optional.
    var targetId = null;
    if (typeof(arguments[nextArg]) == 'string')
      targetId = arguments[nextArg++];

    // connectInfo (second argument) is optional.
    var connectInfo = null;
    if (typeof(arguments[nextArg]) == 'object')
      connectInfo = arguments[nextArg++];

    if (nextArg != arguments.length)
      throw new Error('Invalid arguments to connect');
    return [targetId, connectInfo];
  });

  apiFunctions.setHandleRequest('connect', function(targetId, connectInfo) {
    if (!targetId)
      targetId = extensionId;
    var name = '';
    if (connectInfo && connectInfo.name)
      name = connectInfo.name;

    var portId = OpenChannelToExtension(extensionId, targetId, name);
    if (portId >= 0)
      return chromeHidden.Port.createPort(portId, name);
    throw new Error('Error connecting to extension ' + targetId);
  });
});

})();
