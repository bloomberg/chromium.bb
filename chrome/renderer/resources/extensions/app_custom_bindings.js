// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app API.

var GetAvailability = requireNative('v8_context').GetAvailability;
if (!GetAvailability('app').is_available) {
  exports.chromeApp = {};
  exports.chromeHiddenApp = {};
  return;
}

var appNatives = requireNative('app');
var chrome = requireNative('chrome').GetChrome();
var process = requireNative('process');
var extensionId = process.GetExtensionId();
var logActivity = requireNative('activityLogger');

function wrapForLogging(fun) {
  var id = extensionId;
  return (function() {
    // TODO(ataly): We need to make sure we use the right prototype for
    // fun.apply. Array slice can either be rewritten or similarly defined.
    logActivity.LogAPICall(id, "app." + fun.name,
        Array.prototype.slice.call(arguments));
    return fun.apply(this, arguments);
  });
}

// This becomes chrome.app
var app;
if (!extensionId) {
  app = {
    getIsInstalled: appNatives.GetIsInstalled,
    install: appNatives.Install,
    getDetails: appNatives.GetDetails,
    getDetailsForFrame: appNatives.GetDetailsForFrame,
    runningState: appNatives.GetRunningState
  };
} else {
  app = {
    getIsInstalled: wrapForLogging(appNatives.GetIsInstalled),
    install: wrapForLogging(appNatives.Install),
    getDetails: wrapForLogging(appNatives.GetDetails),
    getDetailsForFrame: wrapForLogging(appNatives.GetDetailsForFrame),
    runningState: wrapForLogging(appNatives.GetRunningState)
  };
}

// Tricky; "getIsInstalled" is actually exposed as the getter "isInstalled",
// but we don't have a way to express this in the schema JSON (nor is it
// worth it for this one special case).
//
// So, define it manually, and let the getIsInstalled function act as its
// documentation.
if (!extensionId)
  app.__defineGetter__('isInstalled', appNatives.GetIsInstalled);
else
  app.__defineGetter__('isInstalled',
                       wrapForLogging(appNatives.GetIsInstalled));

// Called by app_bindings.cc.
// This becomes chromeHidden.app
var chromeHiddenApp = {
  onInstallStateResponse: function(state, callbackId) {
    if (callbackId) {
      callbacks[callbackId](state);
      delete callbacks[callbackId];
    }
  }
};

// TODO(kalman): move this stuff to its own custom bindings.
var callbacks = {};
var nextCallbackId = 1;

app.installState = function getInstallState(callback) {
  var callbackId = nextCallbackId++;
  callbacks[callbackId] = callback;
  appNatives.GetInstallState(callbackId);
};
if (extensionId)
  app.installState = wrapForLogging(app.installState);

// These must match the names in InstallAppbinding() in
// chrome/renderer/extensions/dispatcher.cc.
exports.chromeApp = app;
exports.chromeHiddenApp = chromeHiddenApp;
