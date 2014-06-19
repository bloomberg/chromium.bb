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
  Utils.timeLog('MediaKeySession onMessage', message);
  var initData = Utils.getInitDataFromMessage(message, TestConfig.mediaType);
  var key = Utils.getDefaultKey(TestConfig.forceInvalidResponse);
  var jwkSet = Utils.createJWKData(initData, key);
  message.target.update(jwkSet);
};
