// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = chrome || {};
(function() {
  native function GetChromeHidden();
  native function GetIsInstalled();
  native function Install();
  native function GetDetails();
  native function GetDetailsForFrame();
  native function GetAppNotifyChannel();

  var chromeHidden = GetChromeHidden();
  var callbacks = {};
  var nextCallbackId = 1;

  chrome.app = new function() {
    this.__defineGetter__('isInstalled', GetIsInstalled);
    this.install = Install;
    this.getDetails = GetDetails;
    this.getDetailsForFrame = GetDetailsForFrame;
  }();

  chrome.appNotifications = new function() {
    this.getChannel = function(clientId, callback) {
      var callbackId = 0;
      if (callback) {
        callbackId = nextCallbackId++;
        callbacks[callbackId] = callback;
      }
      GetAppNotifyChannel(clientId, callbackId);
    };
  }();

  chromeHidden.app = {};
  chromeHidden.app.onGetAppNotifyChannelResponse =
      function(channelId, error, callbackId) {
    if (callbackId) {
      callbacks[callbackId](channelId, error);
      delete callbacks[callbackId];
    }
  };
})();
