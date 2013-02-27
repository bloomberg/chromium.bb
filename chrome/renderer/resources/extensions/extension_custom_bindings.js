// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the extension API.

var binding = require('binding').Binding.create('extension');

var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var runtimeNatives = requireNative('runtime');
var OpenChannelToExtension = runtimeNatives.OpenChannelToExtension;
var OpenChannelToNativeApp = runtimeNatives.OpenChannelToNativeApp;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var chrome = requireNative('chrome').GetChrome();
var sendMessageUpdateArguments =
    require('miscellaneous_bindings').sendMessageUpdateArguments;

var inIncognitoContext = requireNative('process').InIncognitoContext();
var sendRequestIsDisabled = requireNative('process').IsSendRequestDisabled();
var contextType = requireNative('process').GetContextType();
var manifestVersion = requireNative('process').GetManifestVersion();

// This should match chrome.windows.WINDOW_ID_NONE.
//
// We can't use chrome.windows.WINDOW_ID_NONE directly because the
// chrome.windows API won't exist unless this extension has permission for it;
// which may not be the case.
var WINDOW_ID_NONE = -1;

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var extension = bindingsAPI.compiledApi;
  if (manifestVersion < 2) {
    chrome.self = extension;
    extension.inIncognitoTab = inIncognitoContext;
  }
  extension.inIncognitoContext = inIncognitoContext;

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

  // Alias several messaging deprecated APIs to their runtime counterparts.
  var mayNeedAlias = [
    // Types
    'Port',
    // Functions
    'connect', 'sendMessage', 'connectNative', 'sendNativeMessage',
    // Events
    'onConnect', 'onConnectExternal', 'onMessage', 'onMessageExternal'
  ];
  mayNeedAlias.forEach(function(alias) {
    // Checking existence isn't enough since some functions are disabled via
    // getters that throw exceptions. Assume that any getter is such a function.
    if (chrome.runtime.hasOwnProperty(alias) &&
        chrome.runtime.__lookupGetter__(alias) === undefined) {
      extension[alias] = chrome.runtime[alias];
    }
  });

  apiFunctions.setUpdateArgumentsPreValidate('sendRequest',
      sendMessageUpdateArguments.bind(null, 'sendRequest'));

  apiFunctions.setHandleRequest('sendRequest',
                                function(targetId, request, responseCallback) {
    if (sendRequestIsDisabled)
      throw new Error(sendRequestIsDisabled);
    var port = chrome.runtime.connect(targetId || extensionId,
                                      {name: chromeHidden.kRequestChannel});
    chromeHidden.Port.sendMessageImpl(port, request, responseCallback);
  });

  if (sendRequestIsDisabled) {
    extension.onRequest.addListener = function() {
      throw new Error(sendRequestIsDisabled);
    };
    if (contextType == 'BLESSED_EXTENSION') {
      extension.onRequestExternal.addListener = function() {
        throw new Error(sendRequestIsDisabled);
      };
    }
  }
});

exports.binding = binding.generate();
