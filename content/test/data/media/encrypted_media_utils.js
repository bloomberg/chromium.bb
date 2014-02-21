// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var keySystem = QueryString.keysystem;
var mediaFile = QueryString.mediafile;
var mediaType = QueryString.mediatype || 'video/webm; codecs="vorbis, vp8"';
var useMSE = QueryString.usemse == 1;

// Default key used to encrypt many media files used in browser tests.
var KEY = new Uint8Array([0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                          0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c]);
// KEY_ID constant used as init data while encrypting test media files.
var KEY_ID = getInitDataFromKeyId("0123456789012345");
// Heart beat message header.
var HEART_BEAT_HEADER = 'HEARTBEAT';
var EXTERNAL_CLEAR_KEY_KEY_SYSTEM = "org.chromium.externalclearkey";
// Note that his URL has been normalized from the one in clear_key_cdm.cc.
var EXTERNAL_CLEAR_KEY_HEARTBEAT_URL =
    'http://test.externalclearkey.chromium.org/';

function isHeartbeatMessage(msg) {
  if (msg.length < HEART_BEAT_HEADER.length)
    return false;
  for (var i = 0; i < HEART_BEAT_HEADER.length; ++i) {
    if (String.fromCharCode(msg[i]) != HEART_BEAT_HEADER[i])
      return false;
  }
  return true;
}

function loadEncryptedMediaFromURL(video) {
  return loadEncryptedMedia(video, mediaFile, keySystem, KEY, useMSE);
}

function loadEncryptedMedia(video, mediaFile, keySystem, key, useMSE,
                            appendSourceCallbackFn) {
  var keyRequested = false;
  var sourceOpened = false;
  // Add properties to enable verification that events occurred.
  video.receivedKeyAdded = false;
  video.receivedHeartbeat = false;
  video.isHeartbeatExpected = keySystem === EXTERNAL_CLEAR_KEY_KEY_SYSTEM;
  video.receivedKeyMessage = false;

  if (!(video && mediaFile && keySystem && key)) {
    failTest('Missing parameters in loadEncryptedMedia().');
    return;
  }

  function onNeedKey(e) {
    if (keyRequested)
      return;
    keyRequested = true;
    console.log('onNeedKey', e);
    try {
      video.webkitGenerateKeyRequest(keySystem, e.initData);
    }
    catch(error) {
      setResultInTitle(error.name);
    }
  }

  function onKeyAdded(e) {
    e.target.receivedKeyAdded = true;
  }

  function onKeyMessage(e) {
    video.receivedKeyMessage = true;
    if (!e.keySystem || e.keySystem != keySystem) {
      failTest('keymessage with unexpected keySystem: ' + e.keySystem);
      return;
    }

    if (!e.sessionId) {
      failTest('keymessage without a sessionId: ' + e.sessionId);
      return;
    }

    if (!e.message) {
      failTest('keymessage without a message: ' + e.message);
      return;
    }

    if (isHeartbeatMessage(e.message)) {
      console.log('onKeyMessage - heartbeat', e);
      e.target.receivedHeartbeat = true;
      verifyHeartbeatMessage(e);
      return;
    }

    // No tested key system returns defaultURL in for key request messages.
    if (e.defaultURL) {
      failTest('keymessage unexpectedly has defaultURL: ' + e.defaultURL);
      return;
    }

    // keymessage in response to generateKeyRequest. Reply with key.
    console.log('onKeyMessage - key request', e);
    var initData = e.message;
    if (mediaType.indexOf('mp4') != -1)
      initData = KEY_ID; // Temporary hack for Clear Key in v0.1.
    video.webkitAddKey(keySystem, key, initData);
  }

  function verifyHeartbeatMessage(e) {
    // Only External Clear Key sends a HEARTBEAT message.
    if (e.keySystem != EXTERNAL_CLEAR_KEY_KEY_SYSTEM) {
      failTest('Unexpected heartbeat from ' + e.keySystem);
      return;
    }

    if (e.defaultURL != EXTERNAL_CLEAR_KEY_HEARTBEAT_URL) {
      failTest('Heartbeat message with unexpected defaultURL: ' + e.defaultURL);
      return;
    }
  }

  video.addEventListener('webkitneedkey', onNeedKey);
  video.addEventListener('webkitkeymessage', onKeyMessage);
  video.addEventListener('webkitkeyerror', function() {
      setResultInTitle("KeyError");
  });
  video.addEventListener('webkitkeyadded', onKeyAdded);
  installTitleEventHandler(video, 'error');

  if (useMSE) {
    var mediaSource = loadMediaSource(mediaFile, mediaType,
                                      appendSourceCallbackFn);
    video.src = window.URL.createObjectURL(mediaSource);
  } else {
    video.src = mediaFile;
  }
}

function getInitDataFromKeyId(keyID) {
  var init_key_id = new Uint8Array(keyID.length);
  for(var i = 0; i < keyID.length; i++) {
    init_key_id[i] = keyID.charCodeAt(i);
  }
  return init_key_id;
}
