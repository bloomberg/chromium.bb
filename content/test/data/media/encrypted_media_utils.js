// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var QueryString = function() {
  // Allows access to query parameters on the URL; e.g., given a URL like:
  //    http://<server>/my.html?test=123&bob=123
  // Parameters can then be accessed via QueryString.test or QueryString.bob.
  var params = {};
  // RegEx to split out values by &.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  function d(s) { return decodeURIComponent(s.replace(/\+/g, ' ')); }
  var match;
  while (match = r.exec(window.location.search.substring(1)))
    params[d(match[1])] = d(match[2]);
  return params;
}();

var keySystem = QueryString.keysystem;
var mediaFile = QueryString.mediafile;
var mediaType = QueryString.mediatype;
if (!mediaType)
  mediaType = 'video/webm; codecs="vorbis, vp8"'
// Default key used to encrypt many media files used in browser tests.
var KEY = new Uint8Array([0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                          0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c]);
// KEY_ID constant used as init data while encrypting test media files.
var KEY_ID = getInitDataFromKeyId("0123456789012345");
// Stores a failure message that is read by the browser test when it fails.
var failMessage = '';
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

function failTest(msg) {
  console.log("failTest('" + msg + "')");
  if (msg instanceof Event)
    failMessage = msg.target + '.' + msg.type;
  else
    failMessage = msg;
  setResultInTitle('FAILED');
}

var titleChanged = false;
function setResultInTitle(title) {
  // If document title is 'ENDED', then update it with new title to possibly
  // mark a test as failure.  Otherwise, keep the first title change in place.
  if (!titleChanged || document.title.toUpperCase() == 'ENDED')
    document.title = title.toUpperCase();
  console.log('Set document title to: ' + title + ', updated title: ' +
              document.title);
  titleChanged = true;
}

function installTitleEventHandler(element, event) {
  element.addEventListener(event, function(e) {
    setResultInTitle(event.toString());
  }, false);
}

function loadEncryptedMediaFromURL(video) {
  return loadEncryptedMedia(video, mediaFile, keySystem, KEY);
}

function loadEncryptedMedia(video, mediaFile, keySystem, key) {
  var keyRequested = false;
  var sourceOpened = false;
  // Add properties to enable verification that events occurred.
  video.receivedKeyAdded = false;
  video.receivedHeartbeat = false;
  video.isHeartbeatExpected = keySystem === EXTERNAL_CLEAR_KEY_KEY_SYSTEM;
  video.receivedKeyMessage = false;

  if (!(video && mediaFile && keySystem && key))
    failTest('Missing parameters in loadEncryptedMedia().');

  function onSourceOpen(e) {
    if (sourceOpened)
      return;
    sourceOpened = true;
    console.log('onSourceOpen', e);
    var srcBuffer =
        mediaSource.addSourceBuffer(mediaType);
    var xhr = new XMLHttpRequest();
    xhr.open('GET', mediaFile);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', function(e) {
      srcBuffer.append(new Uint8Array(e.target.response));
      mediaSource.endOfStream();
    });
    xhr.send();
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
      setResultInTitle("GenerateKeyRequestException");
    }
  }

  function onKeyAdded(e) {
    e.target.receivedKeyAdded = true;
  }

  function onKeyMessage(e) {
    video.receivedKeyMessage = true;
    if (!e.keySystem) {
      failTest('keymessage without a keySystem: ' + e.keySystem);
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

  var mediaSource = new WebKitMediaSource();

  mediaSource.addEventListener('webkitsourceopen', onSourceOpen);
  video.addEventListener('webkitneedkey', onNeedKey);
  video.addEventListener('webkitkeymessage', onKeyMessage);
  video.addEventListener('webkitkeyerror', function() {
      setResultInTitle("WebKitKeyError");
  });
  video.addEventListener('webkitkeyadded', onKeyAdded);
  installTitleEventHandler(video, 'error');

  video.src = window.URL.createObjectURL(mediaSource);
  return mediaSource;
}

function getInitDataFromKeyId(keyID) {
  var init_key_id = new Uint8Array(keyID.length);
  for(var i = 0; i < keyID.length; i++) {
    init_key_id[i] = keyID.charCodeAt(i);
  }
  return init_key_id;
}
