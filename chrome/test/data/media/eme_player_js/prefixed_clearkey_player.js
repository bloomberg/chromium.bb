// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClearKey player responsible for playing media using Clear Key key system and
// prefixed EME API.
function PrefixedClearKeyPlayer() {
}

PrefixedClearKeyPlayer.prototype.init = function(video) {
  InitPrefixedEMEPlayer(this, video);
};

PrefixedClearKeyPlayer.prototype.onWebkitKeyMessage = function(message) {
  if (Utils.isHeartBeatMessage(message.message)) {
   Utils.timeLog('onWebkitKeyMessage - heart beat', message);
    return;
  }
  var initData = message.message;
  // If content uses mp4, then message.message is PSSH data. Instead of parsing
  // that data we hard code the initData.
  if (TestConfig.mediaType.indexOf('mp4') != -1)
    // Temporary hack for Clear Key in v0.1.
    initData = Utils.convertToUint8Array(KEY_ID);
  Utils.timeLog('Adding key to sessionID: ' + message.sessionId);
  message.target.webkitAddKey(TestConfig.keySystem, KEY, initData,
                              message.sessionId);
};
