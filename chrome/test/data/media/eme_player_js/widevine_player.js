// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Widevine player responsible for playing media using Widevine key system
// and EME working draft API.
function WidevinePlayer() {
}

WidevinePlayer.prototype.init = function(video) {
  InitEMEPlayer(this, video);
};

WidevinePlayer.prototype.onMessage = function(message) {
  Utils.timeLog('MediaKeySession onMessage', message);
  var mediaKeySession = message.target;

  function onSuccess(response) {
    var key = new Uint8Array(response);
    Utils.timeLog('Update media key session with license response.', key);
    mediaKeySession.update(key);
  }
  Utils.sendRequest('POST', 'arraybuffer', message.message,
                    TestConfig.licenseServer, onSuccess);
};
