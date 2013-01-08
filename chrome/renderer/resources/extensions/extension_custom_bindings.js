// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the extension API.

var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var runtimeNatives = requireNative('runtime');
var OpenChannelToExtension = runtimeNatives.OpenChannelToExtension;
var OpenChannelToNativeApp = runtimeNatives.OpenChannelToNativeApp;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendMessageUpdateArguments =
    require('miscellaneous_bindings').sendMessageUpdateArguments;

var inIncognitoContext = requireNative('process').InIncognitoContext();
var sendRequestIsDisabled = requireNative('process').IsSendRequestDisabled();
var contextType = requireNative('process').GetContextType();

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
    try {
      // Deliberately accessing runtime[alias] rather than testing its
      // existence, since accessing may throw an exception if this context
      // doesn't have access.
      if (chrome.runtime[alias])
        chrome.extension[alias] = chrome.runtime[alias];
    } catch(e) {}
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
    chrome.extension.onRequest.addListener = function() {
      throw new Error(sendRequestIsDisabled);
    };
    if (contextType == 'BLESSED_EXTENSION') {
      chrome.extension.onRequestExternal.addListener = function() {
        throw new Error(sendRequestIsDisabled);
      };
    }
  }

});
