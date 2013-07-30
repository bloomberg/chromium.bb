// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the runtime API.

var binding = require('binding').Binding.create('runtime');

var extensionNatives = requireNative('extension');
var messaging = require('messaging');
var runtimeNatives = requireNative('runtime');
var unloadEvent = require('unload_event');
var process = requireNative('process');

var backgroundPage = window;
var backgroundRequire = require;
var contextType = process.GetContextType();
if (contextType == 'BLESSED_EXTENSION' ||
    contextType == 'UNBLESSED_EXTENSION') {
  var manifest = runtimeNatives.GetManifest();
  if (manifest.app && manifest.app.background) {
    // Get the background page if one exists. Otherwise, default to the current
    // window.
    backgroundPage = extensionNatives.GetExtensionViews(-1, 'BACKGROUND')[0];
    if (backgroundPage) {
      var GetModuleSystem = requireNative('v8_context').GetModuleSystem;
      backgroundRequire = GetModuleSystem(backgroundPage).require;
    } else {
      backgroundPage = window;
    }
  }
}

// For packaged apps, all windows use the bindFileEntryCallback from the
// background page so their FileEntry objects have the background page's context
// as their own.  This allows them to be used from other windows (including the
// background page) after the original window is closed.
if (window == backgroundPage) {
  var lastError = require('lastError');
  var fileSystemNatives = requireNative('file_system_natives');
  var GetIsolatedFileSystem = fileSystemNatives.GetIsolatedFileSystem;
  var bindDirectoryEntryCallback = function(functionName, apiFunctions) {
    apiFunctions.setCustomCallback(functionName,
        function(name, request, response) {
      if (request.callback && response) {
        var callback = request.callback;
        request.callback = null;

        var fileSystemId = response.fileSystemId;
        var baseName = response.baseName;
        var fs = GetIsolatedFileSystem(fileSystemId);

        try {
          fs.root.getDirectory(baseName, {}, callback, function(fileError) {
            lastError.run('runtime.' + functionName,
                          'Error getting Entry, code: ' + fileError.code,
                          request.stack,
                          callback);
          });
        } catch (e) {
          lastError.run('runtime.' + functionName,
                        'Error: ' + e.stack,
                        request.stack,
                        callback);
        }
      }
    });
  };
} else {
  // Force the runtime API to be loaded in the background page. Using
  // backgroundPageModuleSystem.require('runtime') is insufficient as
  // requireNative is only allowed while lazily loading an API.
  backgroundPage.chrome.runtime;
  var bindDirectoryEntryCallback = backgroundRequire(
      'runtime').bindDirectoryEntryCallback;
}

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

  var sendMessageUpdateArguments = messaging.sendMessageUpdateArguments;
  apiFunctions.setUpdateArgumentsPreValidate('sendMessage',
      $Function.bind(sendMessageUpdateArguments, null, 'sendMessage'));
  apiFunctions.setUpdateArgumentsPreValidate('sendNativeMessage',
      $Function.bind(sendMessageUpdateArguments, null, 'sendNativeMessage'));

  apiFunctions.setHandleRequest('sendMessage',
                                function(targetId, message, responseCallback) {
    var port = runtime.connect(targetId || runtime.id,
        {name: messaging.kMessageChannel});
    messaging.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('sendNativeMessage',
                                function(targetId, message, responseCallback) {
    var port = runtime.connectNative(targetId);
    messaging.sendMessageImpl(port, message, responseCallback);
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
    // Don't let orphaned content scripts communicate with their extension.
    // http://crbug.com/168263
    if (unloadEvent.wasDispatched)
      throw new Error('Error connecting to extension ' + targetId);

    if (!targetId)
      targetId = runtime.id;

    var name = '';
    if (connectInfo && connectInfo.name)
      name = connectInfo.name;

    var portId = runtimeNatives.OpenChannelToExtension(targetId, name);
    if (portId >= 0)
      return messaging.createPort(portId, name);
  });

  //
  // Privileged APIs.
  //
  if (contextType != 'BLESSED_EXTENSION')
    return;

  apiFunctions.setHandleRequest('connectNative',
                                function(nativeAppName) {
    if (!unloadEvent.wasDispatched) {
      var portId = runtimeNatives.OpenChannelToNativeApp(runtime.id,
                                                         nativeAppName);
      if (portId >= 0)
        return messaging.createPort(portId, '');
    }
    throw new Error('Error connecting to native app: ' + nativeAppName);
  });

  apiFunctions.setCustomCallback('getBackgroundPage',
                                 function(name, request, response) {
    if (request.callback) {
      var bg = extensionNatives.GetExtensionViews(-1, 'BACKGROUND')[0] || null;
      request.callback(bg);
    }
    request.callback = null;
  });

  bindDirectoryEntryCallback('getPackageDirectoryEntry', apiFunctions);
});

exports.bindDirectoryEntryCallback = bindDirectoryEntryCallback;
exports.binding = binding.generate();
