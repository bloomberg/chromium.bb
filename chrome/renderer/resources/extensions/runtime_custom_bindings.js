// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the runtime API.

var runtimeNatives = requireNative('runtime');
var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var OpenChannelToExtension = runtimeNatives.OpenChannelToExtension;
var OpenChannelToNativeApp = runtimeNatives.OpenChannelToNativeApp;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendMessageUpdateArguments =
    require('miscellaneous_bindings').sendMessageUpdateArguments;

chromeHidden.registerCustomHook('runtime', function(bindings, id, contextType) {
  var apiFunctions = bindings.apiFunctions;

  //
  // Unprivileged APIs.
  //

  chrome.runtime.id = id;

  apiFunctions.setHandleRequest('getManifest', function() {
    return runtimeNatives.GetManifest();
  });

  apiFunctions.setHandleRequest('getURL', function(path) {
    path = String(path);
    if (!path.length || path[0] != '/')
      path = '/' + path;
    return 'chrome-extension://' + id + path;
  });

  apiFunctions.setUpdateArgumentsPreValidate('sendMessage',
      sendMessageUpdateArguments.bind(null, 'sendMessage'));
  apiFunctions.setUpdateArgumentsPreValidate('sendNativeMessage',
      sendMessageUpdateArguments.bind(null, 'sendNativeMessage'));

  apiFunctions.setHandleRequest('sendMessage',
                                function(targetId, message, responseCallback) {
    var port = chrome.runtime.connect(targetId || chrome.runtime.id,
        {name: chromeHidden.kMessageChannel});
    chromeHidden.Port.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('sendNativeMessage',
                                function(targetId, message, responseCallback) {
    var port = chrome.runtime.connectNative(
        targetId, message, chromeHidden.kNativeMessageChannel);
    chromeHidden.Port.sendMessageImpl(port, '', responseCallback);
  });

  apiFunctions.setUpdateArgumentsPreValidate('connect', function() {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   runtime.connect()   -> runtime.connect(null, null)
    //   runtime.connect({}) -> runtime.connect(null, {})
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
      targetId = chrome.runtime.id;
    var name = '';
    if (connectInfo && connectInfo.name)
      name = connectInfo.name;

    var portId = OpenChannelToExtension(chrome.runtime.id, targetId, name);
    if (portId >= 0)
      return chromeHidden.Port.createPort(portId, name);
    throw new Error('Error connecting to extension ' + targetId);
  });

  //
  // Privileged APIs.
  //
  if (contextType != 'BLESSED_EXTENSION')
    return;

  apiFunctions.setHandleRequest('connectNative',
                                function(nativeAppName, connectInfo) {
    // Turn the object into a string here, because it eventually will be.
    var portId = OpenChannelToNativeApp(chrome.runtime.id,
                                        nativeAppName,
                                        connectInfo.name,
                                        JSON.stringify(connectInfo.message));
    if (portId >= 0) {
      return chromeHidden.Port.createPort(portId, connectInfo.name);
    }
    throw new Error('Error connecting to native app: ' + nativeAppName);
  });

  apiFunctions.setCustomCallback('getBackgroundPage',
                                 function(name, request, response) {
    if (request.callback) {
      var bg = GetExtensionViews(-1, 'BACKGROUND')[0] || null;
      request.callback(bg);
    }
    request.callback = null;
  });

});
