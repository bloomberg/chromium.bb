// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app API.

var appNatives = requireNative('app');
var chrome = requireNative('chrome').GetChrome();
var GetAvailability = requireNative('v8_context').GetAvailability;

// This becomes chrome.app
var app = {
  getIsInstalled: appNatives.GetIsInstalled,
  install: appNatives.Install,
  getDetails: appNatives.GetDetails,
  getDetailsForFrame: appNatives.GetDetailsForFrame,
  runningState: appNatives.GetRunningState
};

// Tricky; "getIsInstalled" is actually exposed as the getter "isInstalled",
// but we don't have a way to express this in the schema JSON (nor is it
// worth it for this one special case).
//
// So, define it manually, and let the getIsInstalled function act as its
// documentation.
app.__defineGetter__('isInstalled', appNatives.GetIsInstalled);

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

// These must match the names in InstallAppbinding() in
// chrome/renderer/extensions/dispatcher.cc.
var availability = GetAvailability('app');
if (availability.is_available) {
  exports.chromeApp = app;
  exports.chromeHiddenApp = chromeHiddenApp;
}
