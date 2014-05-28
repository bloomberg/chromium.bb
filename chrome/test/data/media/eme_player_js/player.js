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
    'error': 'onError',
    'abort': 'onAbort'
  };

  // List of events that fail tests.
  var failingEvents = ['error', 'onAbort'];

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
  player.onNeedKey = function(message) {
    Utils.timeLog('Creating new media key session for contentType: ' +
                  message.contentType + ', initData: ' +
                  Utils.getHexString(message.initData));
    try {
      var mediaKeySession = message.target.mediaKeys.createSession(
          message.contentType, message.initData);
      mediaKeySession.addEventListener('message', player.onMessage);
      mediaKeySession.addEventListener('error', function(error) {
        Utils.failTest(error);
      });
    } catch (e) {
      Utils.failTest(e);
    }
  };

  InitPlayer(player, video);
  try {
    Utils.timeLog('Setting video media keys: ' + TestConfig.keySystem);
    video.setMediaKeys(new MediaKeys(TestConfig.keySystem));
  } catch (e) {
    Utils.failTest(e);
  }
}

function InitPrefixedEMEPlayer(player, video) {
  player.onWebkitNeedKey = function(message) {
    Utils.timeLog(TestConfig.keySystem + ' Generate key request, initData: ' +
                  Utils.getHexString(message.initData));
    message.target.webkitGenerateKeyRequest(
        TestConfig.keySystem, message.initData);
  };

  player.onWebkitKeyAdded = function(message) {
    Utils.timeLog('onWebkitKeyAdded', message);
    message.target.hasKeyAdded = true;
  };

  InitPlayer(player, video);
}
