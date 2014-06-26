// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Widevine player responsible for playing media using Widevine key system
// and EME working draft API.
function WidevinePlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

WidevinePlayer.prototype.init = function() {
  PlayerUtils.initEMEPlayer(this);
};

WidevinePlayer.prototype.registerEventListeners = function() {
  PlayerUtils.registerEMEEventListeners(this);
};

WidevinePlayer.prototype.onMessage = function(message) {
  Utils.timeLog('MediaKeySession onMessage', message);
  var mediaKeySession = message.target;
  function onSuccess(response) {
    var key = new Uint8Array(response);
    Utils.timeLog('Update media key session with license response.', key);
    if (PROMISES_SUPPORTED) {
      mediaKeySession.update(key).catch(function(error) {
        Utils.failTest(error, KEY_ERROR);
      });
    } else {
      mediaKeySession.update(key);
    }

  }
  Utils.sendRequest('POST', 'arraybuffer', message.message,
                    this.testConfig.licenseServerURL, onSuccess,
                    this.testConfig.forceInvalidResponse);
};
