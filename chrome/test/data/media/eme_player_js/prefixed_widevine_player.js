// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Widevine player responsible for playing media using Widevine key system
// and prefixed EME API.
function PrefixedWidevinePlayer() {
}

PrefixedWidevinePlayer.prototype.init = function(video) {
  InitPrefixedEMEPlayer(this, video);
};

PrefixedWidevinePlayer.prototype.onWebkitKeyMessage = function(message) {
  function onSuccess(response) {
    Utils.timeLog('Got license response', key);
    var key = new Uint8Array(response);
    Utils.timeLog('Adding key to sessionID: ' + message.sessionId);
    message.target.webkitAddKey(TestConfig.keySystem, key, new Uint8Array(1),
                                message.sessionId);
  }
  Utils.sendRequest('POST', 'arraybuffer', message.message,
                    TestConfig.licenseServer, onSuccess);
};
