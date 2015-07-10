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
    'encrypted': 'onEncrypted',
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

  player.video.addEventListener('error', function(error) {
    // This most likely happens on pipeline failures (e.g. when the CDM
    // crashes).
    Utils.timeLog('onHTMLElementError', error);
    Utils.failTest(error);
  });
};

// Register the necessary event handlers needed when playing encrypted content
// using the unprefixed API. Returns a promise that resolves to the player.
PlayerUtils.registerEMEEventListeners = function(player) {
  player.video.addEventListener('encrypted', function(message) {

    function addMediaKeySessionListeners(mediaKeySession) {
      mediaKeySession.addEventListener('message', function(message) {
        player.video.receivedKeyMessage = true;
        if (Utils.isRenewalMessage(message)) {
          Utils.timeLog('MediaKeySession onMessage - renewal', message);
          player.video.receivedRenewalMessage = true;
        } else {
          if (message.messageType != 'license-request') {
            Utils.failTest('Unexpected message type "' + message.messageType +
                               '" received.',
                           EME_MESSAGE_UNEXPECTED_TYPE);
          }
        }
        player.onMessage(message);
      });
    }

    try {
      if (player.testConfig.sessionToLoad) {
        Utils.timeLog('Loading session: ' + player.testConfig.sessionToLoad);
        var session =
            message.target.mediaKeys.createSession('persistent-license');
        addMediaKeySessionListeners(session);
        session.load(player.testConfig.sessionToLoad)
            .then(
                function(result) {
                  if (!result)
                    Utils.failTest('Session not found.', EME_SESSION_NOT_FOUND);
                },
                function(error) { Utils.failTest(error, EME_LOAD_FAILED); });
      } else {
        Utils.timeLog('Creating new media key session for initDataType: ' +
                      message.initDataType + ', initData: ' +
                      Utils.getHexString(new Uint8Array(message.initData)));
        var session = message.target.mediaKeys.createSession();
        addMediaKeySessionListeners(session);
        session.generateRequest(message.initDataType, message.initData)
            .catch(function(error) {
              // Ignore the error if a crash is expected. This ensures that
              // the decoder actually detects and reports the error.
              if (this.testConfig.keySystem !=
                  'org.chromium.externalclearkey.crash') {
                Utils.failTest(error, EME_GENERATEREQUEST_FAILED);
              }
            });
      }
    } catch (e) {
      Utils.failTest(e);
    }
  });

  this.registerDefaultEventListeners(player);
  player.video.receivedKeyMessage = false;
  Utils.timeLog('Setting video media keys: ' + player.testConfig.keySystem);
  var config = {};
  if (player.testConfig.sessionToLoad) {
    config = {
        persistentState: "required",
        sessionTypes: ["temporary", "persistent-license"]
    };
  }
  return navigator.requestMediaKeySystemAccess(
      player.testConfig.keySystem, [config])
      .then(function(access) { return access.createMediaKeys(); })
      .then(function(mediaKeys) {
        return player.video.setMediaKeys(mediaKeys);
      })
      .then(function(result) { return player; })
      .catch(function(error) { Utils.failTest(error, NOTSUPPORTEDERROR); });
};

// Register the necessary event handlers needed when playing encrypted content
// using the prefixed API. Even though the prefixed API is all synchronous,
// returns a promise that resolves to the player.
PlayerUtils.registerPrefixedEMEEventListeners = function(player) {
 player.video.addEventListener('webkitneedkey', function(message) {
    var initData = message.initData;
    if (player.testConfig.sessionToLoad) {
      Utils.timeLog('Loading session: ' + player.testConfig.sessionToLoad);
      initData =
          Utils.convertToUint8Array(PREFIXED_EME_API_LOAD_SESSION_HEADER +
                                    player.testConfig.sessionToLoad);
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
    Utils.timeLog('onWebkitKeyError',
                  'KeySystem: ' + error.keySystem + ', sessionId: ' +
                      error.sessionId + ', errorCode: ' + error.errorCode.code +
                      ', systemCode: ' + error.systemCode);
    Utils.failTest(error, PREFIXED_EME_ERROR_EVENT);
  });

  player.video.addEventListener('webkitkeymessage', function(message) {
    Utils.timeLog('onWebkitKeyMessage', message);
    message.target.receivedKeyMessage = true;
    if (Utils.isRenewalMessagePrefixed(message.message)) {
      Utils.timeLog('onWebkitKeyMessage - renewal', message);
      message.target.receivedRenewalMessage = true;
    }
  });

  // The prefixed API is all synchronous, so wrap the calls in a promise.
  return new Promise(function(resolve, reject) {
    PlayerUtils.registerDefaultEventListeners(player);
    player.video.receivedKeyMessage = false;
    resolve(player);
  });
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

// Initialize the player to play encrypted content. Returns a promise that
// resolves to the player.
PlayerUtils.initEMEPlayer = function(player) {
  return player.registerEventListeners().then(function(result) {
    PlayerUtils.setVideoSource(player);
    return player;
  });
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
