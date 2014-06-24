// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Widevine player responsible for playing media using Widevine key system
// and prefixed EME API.
function PrefixedWidevinePlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

PrefixedWidevinePlayer.prototype.init = function() {
  PlayerUtils.initPrefixedEMEPlayer(this);
};

PrefixedWidevinePlayer.prototype.registerEventListeners = function() {
  PlayerUtils.registerPrefixedEMEEventListeners(this);
};

PrefixedWidevinePlayer.prototype.onWebkitKeyMessage = function(message) {
  function onSuccess(response) {
    var key = new Uint8Array(response);
    Utils.timeLog('Adding key to sessionID: ' + message.sessionId, key);
    message.target.webkitAddKey(this.testConfig.keySystem,
                                key,
                                new Uint8Array(1),
                                message.sessionId);
  }
  Utils.sendRequest('POST', 'arraybuffer', message.message,
                    this.testConfig.licenseServerURL, onSuccess,
                    this.testConfig.forceInvalidResponse);
};
