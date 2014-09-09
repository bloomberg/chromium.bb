// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The PlayerUtils provides utility functions to binding common media events
// to specific player functions. It also provides functions to load media source
// base on test configurations.
var PlayerUtils = new function() {
}

// Prepares a video element for playback by setting default event handlers
// and source attribute.
PlayerUtils.registerDefaultEventListeners = function(player) {
  Utils.timeLog('Registering video event handlers.');
  // Map from event name to event listener function name.  It is common for
  // event listeners to be named onEventName.
  var eventListenerMap = {
    'needkey': 'onNeedKey',
    'webkitneedkey': 'onWebkitNeedKey',
    'webkitkeymessage': 'onWebkitKeyMessage',
    'webkitkeyadded': 'onWebkitKeyAdded',
    'webkitkeyerror': 'onWebkitKeyError'
  };
  for (eventName in eventListenerMap) {
    var eventListenerFunction = player[eventListenerMap[eventName]];
    if (eventListenerFunction) {
      player.video.addEventListener(eventName, function(e) {
        player[eventListenerMap[e.type]](e);
      });
    }
  }
  // List of events that fail tests.
  var failingEvents = ['error', 'abort'];
  for (var i = 0; i < failingEvents.length; i++) {
    player.video.addEventListener(failingEvents[i], Utils.failTest);
  }
};

PlayerUtils.registerEMEEventListeners = function(player) {
  player.video.addEventListener('needkey', function(message) {

    function addMediaKeySessionListeners(mediaKeySession) {
      mediaKeySession.addEventListener('message', function(message) {
        player.video.receivedKeyMessage = true;
        if (Utils.isHeartBeatMessage(message.message)) {
          Utils.timeLog('MediaKeySession onMessage - heart beat', message);
          player.video.receivedHeartbeat = true;
        }
        player.onMessage(message);
      });
      mediaKeySession.addEventListener('error', function(error) {
        Utils.failTest(error, KEY_ERROR);
      });
    }

    Utils.timeLog('Creating new media key session for contentType: ' +
                  message.contentType + ', initData: ' +
                  Utils.getHexString(message.initData));
    try {
      if (message.target.mediaKeys.createSession.length == 0) {
        // FIXME(jrummell): Remove this test (and else branch) once blink
        // uses the new API.
        var session = message.target.mediaKeys.createSession();
        addMediaKeySessionListeners(session);
        session.generateRequest(message.contentType, message.initData)
          .catch(function(error) {
            Utils.failTest(error, KEY_ERROR);
          });
      } else {
        var session = message.target.mediaKeys.createSession(
            message.contentType, message.initData);
        session.then(addMediaKeySessionListeners)
            .catch(function(error) {
              Utils.failTest(error, KEY_ERROR);
            });
      }
    } catch (e) {
      Utils.failTest(e);
    }
  });
  this.registerDefaultEventListeners(player);
  try {
    Utils.timeLog('Setting video media keys: ' + player.testConfig.keySystem);
    MediaKeys.create(player.testConfig.keySystem).then(function(mediaKeys) {
      player.video.setMediaKeys(mediaKeys);
    }).catch(function(error) {
      Utils.failTest(error, NOTSUPPORTEDERROR);
    });
  } catch (e) {
    Utils.failTest(e);
  }
};

PlayerUtils.registerPrefixedEMEEventListeners = function(player) {
 player.video.addEventListener('webkitneedkey', function(message) {
    var initData = message.initData;
    if (player.testConfig.sessionToLoad) {
      Utils.timeLog('Loading session: ' + player.testConfig.sessionToLoad);
      initData = Utils.convertToUint8Array(
          PREFIXED_API_LOAD_SESSION_HEADER + player.testConfig.sessionToLoad);
    }
    Utils.timeLog(player.testConfig.keySystem +
                  ' Generate key request, initData: ' +
                  Utils.getHexString(initData));
    try {
      message.target.webkitGenerateKeyRequest(player.testConfig.keySystem,
                                              initData);
    } catch (e) {
      Utils.failTest(e);
    }
  });

  player.video.addEventListener('webkitkeyadded', function(message) {
    Utils.timeLog('onWebkitKeyAdded', message);
    message.target.receivedKeyAdded = true;
  });

  player.video.addEventListener('webkitkeyerror', function(error) {
    Utils.timeLog('onWebkitKeyError', error);
    Utils.failTest(error, KEY_ERROR);
  });

  player.video.addEventListener('webkitkeymessage', function(message) {
    Utils.timeLog('onWebkitKeyMessage', message);
    message.target.receivedKeyMessage = true;
    if (Utils.isHeartBeatMessage(message.message)) {
      Utils.timeLog('onWebkitKeyMessage - heart beat', message);
      message.target.receivedHeartbeat = true;
    }
  });
  this.registerDefaultEventListeners(player);
};

PlayerUtils.setVideoSource = function(player) {
  if (player.testConfig.useMSE) {
    Utils.timeLog('Loading media using MSE.');
    var mediaSource =
        MediaSourceUtils.loadMediaSourceFromTestConfig(player.testConfig);
    player.video.src = window.URL.createObjectURL(mediaSource);
  } else {
    Utils.timeLog('Loading media using src.');
    player.video.src = player.testConfig.mediaFile;
  }
};

PlayerUtils.initEMEPlayer = function(player) {
  this.registerEMEEventListeners(player);
  this.setVideoSource(player);
};

PlayerUtils.initPrefixedEMEPlayer = function(player) {
  this.registerPrefixedEMEEventListeners(player);
  this.setVideoSource(player);
};

// Return the appropriate player based on test configuration.
PlayerUtils.createPlayer = function(video, testConfig) {
  // Update keySystem if using prefixed Clear Key since it is not available as a
  // separate key system to choose from; however it can be set in URL query.
  var usePrefixedEME = testConfig.usePrefixedEME;
  if (testConfig.keySystem == CLEARKEY && usePrefixedEME)
    testConfig.keySystem = PREFIXED_CLEARKEY;

  function getPlayerType(keySystem) {
    switch (keySystem) {
      case WIDEVINE_KEYSYSTEM:
        if (usePrefixedEME)
          return PrefixedWidevinePlayer;
        return WidevinePlayer;
      case PREFIXED_CLEARKEY:
        return PrefixedClearKeyPlayer;
      case EXTERNAL_CLEARKEY:
      case CLEARKEY:
        if (usePrefixedEME)
          return PrefixedClearKeyPlayer;
        return ClearKeyPlayer;
      case FILE_IO_TEST_KEYSYSTEM:
        if (usePrefixedEME)
          return FileIOTestPlayer;
      default:
        Utils.timeLog(keySystem + ' is not a known key system');
        if (usePrefixedEME)
          return PrefixedClearKeyPlayer;
        return ClearKeyPlayer;
    }
  }
  var Player = getPlayerType(testConfig.keySystem);
  return new Player(video, testConfig);
};
