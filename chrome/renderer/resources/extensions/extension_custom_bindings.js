// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the extension API.

var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var OpenChannelToExtension = extensionNatives.OpenChannelToExtension;
var OpenChannelToNativeApp = extensionNatives.OpenChannelToNativeApp;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

var inIncognitoContext = requireNative('process').InIncognitoContext();

chrome.extension = chrome.extension || {};

var manifestVersion = requireNative('process').GetManifestVersion();
if (manifestVersion < 2) {
  chrome.self = chrome.extension;
  chrome.extension.inIncognitoTab = inIncognitoContext;
}

chrome.extension.inIncognitoContext = inIncognitoContext;

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

  function sendMessageUpdateArguments(functionName) {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   extension.sendRequest(req)     -> extension.sendRequest(null, req)
    //   extension.sendRequest(req, cb) -> extension.sendRequest(null, req, cb)
    var args = Array.prototype.splice.call(arguments, 1);  // skip functionName
    var lastArg = args.length - 1;

    // responseCallback (last argument) is optional.
    var responseCallback = null;
    if (typeof(args[lastArg]) == 'function')
      responseCallback = args[lastArg--];

    // request (second argument) is required.
    var request = args[lastArg--];

    // targetId (first argument, extensionId in the manfiest) is optional.
    var targetId = null;
    if (lastArg >= 0)
      targetId = args[lastArg--];

    if (lastArg != -1)
      throw new Error('Invalid arguments to ' + functionName + '.');
    return [targetId, request, responseCallback];
  }
  apiFunctions.setUpdateArgumentsPreValidate('sendRequest',
      sendMessageUpdateArguments.bind(null, 'sendRequest'));
  apiFunctions.setUpdateArgumentsPreValidate('sendMessage',
      sendMessageUpdateArguments.bind(null, 'sendMessage'));
  apiFunctions.setUpdateArgumentsPreValidate('sendNativeMessage',
      sendMessageUpdateArguments.bind(null, 'sendNativeMessage'));

  apiFunctions.setHandleRequest('sendRequest',
                                function(targetId, request, responseCallback) {
    var port = chrome.extension.connect(targetId || extensionId,
                                        {name: chromeHidden.kRequestChannel});
    chromeHidden.Port.sendMessageImpl(port, request, responseCallback);
  });

  apiFunctions.setHandleRequest('sendMessage',
                                function(targetId, message, responseCallback) {
    var port = chrome.extension.connect(targetId || extensionId,
                                        {name: chromeHidden.kMessageChannel});
    chromeHidden.Port.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('sendNativeMessage',
                                function(targetId, message, responseCallback) {
    var port = chrome.extension.connectNative(
        targetId, message, chromeHidden.kNativeMessageChannel);
    chromeHidden.Port.sendMessageImpl(port, '', responseCallback);
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
      throw new Error('Invalid arguments to connect.');
    return [targetId, connectInfo];
  });

  apiFunctions.setUpdateArgumentsPreValidate('connectNative', function() {
    var nextArg = 0;

    // appName is required.
    var appName = arguments[nextArg++];

    // connectionMessage is required.
    var connectMessage = arguments[nextArg++];

    // channelName is only passed by sendMessage
    var channelName = 'connectNative';
    if (typeof(arguments[nextArg]) == 'string')
      channelName = arguments[nextArg++];

    if (nextArg != arguments.length)
      throw new Error('Invalid arguments to connectNative.');
    return [appName, {name: channelName, message: connectMessage}];
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

  apiFunctions.setHandleRequest('connectNative',
                                function(nativeAppName, connectInfo) {
    // Turn the object into a string here, because it eventually will be.
    var portId = OpenChannelToNativeApp(extensionId,
                                        nativeAppName,
                                        connectInfo.name,
                                        JSON.stringify(connectInfo.message));
    if (portId >= 0) {
      return chromeHidden.Port.createPort(portId, connectInfo.name);
    }
    throw new Error('Error connecting to native app: ' + nativeAppName);
  });
});
