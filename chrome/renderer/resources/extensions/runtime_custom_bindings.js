// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the runtime API.

var binding = require('binding').Binding.create('runtime');

var runtimeNatives = requireNative('runtime');
var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var OpenChannelToExtension = runtimeNatives.OpenChannelToExtension;
var OpenChannelToNativeApp = runtimeNatives.OpenChannelToNativeApp;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendMessageUpdateArguments =
    require('miscellaneous_bindings').sendMessageUpdateArguments;

binding.registerCustomHook(function(binding, id, contextType) {
  var apiFunctions = binding.apiFunctions;
  var runtime = binding.compiledApi;

  //
  // Unprivileged APIs.
  //

  runtime.id = id;

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
    var port = runtime.connect(targetId || runtime.id,
        {name: chromeHidden.kMessageChannel});
    chromeHidden.Port.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('sendNativeMessage',
                                function(targetId, message, responseCallback) {
    var port = runtime.connectNative(targetId);
    chromeHidden.Port.sendMessageImpl(port, message, responseCallback);
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

  apiFunctions.setUpdateArgumentsPreValidate('connectNative',
                                             function(appName) {
    if (typeof(appName) !== 'string') {
      throw new Error('Invalid arguments to connectNative.');
    }
    return [appName];
  });

  apiFunctions.setHandleRequest('connect', function(targetId, connectInfo) {
    if (!targetId)
      targetId = runtime.id;
    var name = '';
    if (connectInfo && connectInfo.name)
      name = connectInfo.name;

    // Don't let orphaned content scripts communicate with their extension.
    // http://crbug.com/168263
    if (!chromeHidden.wasUnloaded) {
      var portId = OpenChannelToExtension(runtime.id, targetId, name);
      if (portId >= 0)
        return chromeHidden.Port.createPort(portId, name);
    }
    throw new Error('Error connecting to extension ' + targetId);
  });

  //
  // Privileged APIs.
  //
  if (contextType != 'BLESSED_EXTENSION')
    return;

  apiFunctions.setHandleRequest('connectNative',
                                function(nativeAppName) {
    if (!chromeHidden.wasUnloaded) {
      var portId = OpenChannelToNativeApp(runtime.id, nativeAppName);
      if (portId >= 0)
        return chromeHidden.Port.createPort(portId, '');
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

exports.binding = binding.generate();
