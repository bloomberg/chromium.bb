// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the app API.

var appNatives = requireNative('app');
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chrome.app = {
  getIsInstalled: appNatives.GetIsInstalled,
  install: appNatives.Install,
  getDetails: appNatives.GetDetails,
  getDetailsForFrame: appNatives.GetDetailsForFrame
};

// Tricky; "getIsInstalled" is actually exposed as the getter "isInstalled",
// but we don't have a way to express this in the schema JSON (nor is it
// worth it for this one special case).
//
// So, define it manually, and let the getIsInstalled function act as its
// documentation.
chrome.app.__defineGetter__('isInstalled', appNatives.GetIsInstalled);

// Called by app_bindings.cc.
chromeHidden.app = {
  onGetAppNotifyChannelResponse: function(channelId, error, callbackId) {
    if (callbackId) {
      callbacks[callbackId](channelId, error);
      delete callbacks[callbackId];
    }
  }
};

// appNotification stuff.
//
// TODO(kalman): move this stuff to its own custom bindings.
// It will be bit tricky since I'll need to look into why there are
// permissions defined for app notifications, yet this always sets it up?
var callbacks = {};
var nextCallbackId = 1;

chrome.appNotifications = {
  getChannel: function getChannel(clientId, callback) {
    var callbackId = 0;
    if (callback) {
      callbackId = nextCallbackId++;
      callbacks[callbackId] = callback;
    }
    appNatives.GetAppNotifyChannel(clientId, callbackId);
  }
};
