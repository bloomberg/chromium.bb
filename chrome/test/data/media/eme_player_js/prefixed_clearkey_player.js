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
  var initData =  Utils.getInitDataFromMessage(message, TestConfig.mediaType);
  var key = Utils.getDefaultKey(TestConfig.forceInvalidResponse);
  Utils.timeLog('Adding key to sessionID: ' + message.sessionId);
  message.target.webkitAddKey(TestConfig.keySystem, key, initData,
                              message.sessionId);
};
