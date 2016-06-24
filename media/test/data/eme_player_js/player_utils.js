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

// Register the necessary event handlers needed when playing encrypted content.
// Returns a promise that resolves to the player.
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
  // The File IO test requires persistent state support.
  if (player.testConfig.keySystem ==
      'org.chromium.externalclearkey.fileiotest') {
    config = {persistentState: "required"};
  }
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
  Utils.timeLog('video.src has been set to ' + player.video.src);
};

// Initialize the player to play encrypted content. Returns a promise that
// resolves to the player.
PlayerUtils.initEMEPlayer = function(player) {
  return player.registerEventListeners().then(function(result) {
    PlayerUtils.setVideoSource(player);
    Utils.timeLog('initEMEPlayer() done');
    return player;
  });
};

// Return the appropriate player based on test configuration.
PlayerUtils.createPlayer = function(video, testConfig) {
  function getPlayerType(keySystem) {
    switch (keySystem) {
      case WIDEVINE_KEYSYSTEM:
        return WidevinePlayer;
      case EXTERNAL_CLEARKEY:
      case CLEARKEY:
        return ClearKeyPlayer;
      case FILE_IO_TEST_KEYSYSTEM:
        return FileIOTestPlayer;
      default:
        Utils.timeLog(keySystem + ' is not a known key system');
        return ClearKeyPlayer;
    }
  }
  var Player = getPlayerType(testConfig.keySystem);
  return new Player(video, testConfig);
};
