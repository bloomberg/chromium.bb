// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Prepares a video element for playback by setting default event handlers
// and source attribute.
function InitPlayer(player, video) {
  Utils.timeLog('Registering video event handlers.');

  // Map from event name to event listener function name.  It is common for
  // event listeners to be named onEventName.
  var eventListenerMap = {
    'needkey': 'onNeedKey',
    'webkitneedkey': 'onWebkitNeedKey',
    'webkitkeymessage': 'onWebkitKeyMessage',
    'webkitkeyadded': 'onWebkitKeyAdded',
    'webkitkeyerror': 'onWebkitKeyError',
    'error': 'onError',
    'abort': 'onAbort'
  };

  // List of events that fail tests.
  var failingEvents = ['error', 'abort'];

  for (eventName in eventListenerMap) {
    video.addEventListener(eventName,
                           player[eventListenerMap[eventName]] || function(){});
  }

  for (var i = 0; i < failingEvents.length; i++) {
    video.addEventListener(failingEvents[i], Utils.failTest);
  }

  if (TestConfig.useSRC) {
    Utils.timeLog('Loading media using src.');
    video.src = TestConfig.mediaFile;
  } else {
    var mediaSource = MediaSourceUtils.loadMediaSourceForTest();
    video.src = window.URL.createObjectURL(mediaSource);
  }
}

function InitEMEPlayer(player, video) {
  video.addEventListener('needkey', function(message) {
    Utils.timeLog('Creating new media key session for contentType: ' +
                  message.contentType + ', initData: ' +
                  Utils.getHexString(message.initData));
    try {
      var mediaKeySession = message.target.mediaKeys.createSession(
          message.contentType, message.initData);
      mediaKeySession.addEventListener('message', player.onMessage);
      mediaKeySession.addEventListener('message', function(message) {
        video.receivedKeyMessage = true;
        if (Utils.isHeartBeatMessage(message.message)) {
          Utils.timeLog('MediaKeySession onMessage - heart beat', message);
          video.receivedHeartbeat = true;
        }
      });
      mediaKeySession.addEventListener('error', function(error) {
        Utils.failTest(error, KEY_ERROR);
      });
    } catch (e) {
      Utils.failTest(e);
    }
  });

  InitPlayer(player, video);
  try {
    Utils.timeLog('Setting video media keys: ' + TestConfig.keySystem);
    video.setMediaKeys(new MediaKeys(TestConfig.keySystem));
  } catch (e) {
    Utils.failTest(e);
  }
}

function InitPrefixedEMEPlayer(player, video) {
  video.addEventListener('webkitneedkey', function(message) {
    var initData = message.initData;
    if (TestConfig.sessionToLoad) {
      Utils.timeLog('Loading session: ' + TestConfig.sessionToLoad)
      initData = Utils.convertToUint8Array(
          PREFIXED_API_LOAD_SESSION_HEADER + TestConfig.sessionToLoad);
    }
    Utils.timeLog(TestConfig.keySystem + ' Generate key request, initData: ' +
                  Utils.getHexString(initData));
    try {
      message.target.webkitGenerateKeyRequest(TestConfig.keySystem, initData);
    } catch(e) {
      Utils.failTest(e);
    }
  });

  video.addEventListener('webkitkeyadded', function(message) {
    Utils.timeLog('onWebkitKeyAdded', message);
    message.target.receivedKeyAdded = true;
  });

  video.addEventListener('webkitkeyerror', function(error) {
    Utils.timeLog('onWebkitKeyError', error);
    Utils.failTest(error, KEY_ERROR);
  });

  video.addEventListener('webkitkeymessage', function(message) {
    Utils.timeLog('onWebkitKeyMessage', message);
    message.target.receivedKeyMessage = true;
    if (Utils.isHeartBeatMessage(message.message)) {
      Utils.timeLog('onWebkitKeyMessage - heart beat', message);
      message.target.receivedHeartbeat = true;
    }
  });

  InitPlayer(player, video);
}
