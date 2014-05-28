// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClearKeyPlayer responsible for playing media using Clear Key key system and
// the unprefixed version of EME.
function ClearKeyPlayer() {
}

ClearKeyPlayer.prototype.init = function(video) {
  InitEMEPlayer(this, video);
};

ClearKeyPlayer.prototype.onMessage = function(message) {
  if (Utils.isHeartBeatMessage(message.message)) {
    Utils.timeLog('MediaKeySession onMessage - heart beat', message);
    return;
  }
  Utils.timeLog('MediaKeySession onMessage', message);
  var initData = message.message;
  // If content uses mp4, then message.message is pssh data. Instead of parsing
  // that data we hard code the initData.
  if (TestConfig.mediaType.indexOf('mp4') != -1)
    // Temporary hack for Clear Key in v0.1.
    initData = Utils.convertToUint8Array(KEY_ID);
  var jwkSet = Utils.createJWKData(initData, KEY);
  message.target.update(jwkSet);
};
