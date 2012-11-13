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
// Stores a failure message that is read by the browser test when it fails.
var failMessage = '';
// Heart beat message header.
var HEART_BEAT_HEADER = 'HEARTBEAT';

function isHeartBeatMessage(msg) {
  if (msg.length < HEART_BEAT_HEADER.length)
    return false;

  for (var i = 0; i < HEART_BEAT_HEADER.length; ++i) {
    if (HEART_BEAT_HEADER[i] != String.fromCharCode(msg[i]))
      return false;
  }

  return true;
}

function failTest(msg) {
  if (msg instanceof Event)
    failMessage = msg.target + '.' + msg.type;
  else
    failMessage = msg;
  setDocTitle('FAILED');
}

function setDocTitle(title) {
  document.title = title.toUpperCase();
}

function installTitleEventHandler(element, event) {
  element.addEventListener(event, function(e) {
    setDocTitle(event.toString());
  }, false);
}

function loadEncryptedMediaFromURL(video) {
  return loadEncryptedMedia(video, mediaFile, keySystem, KEY);
}

function loadEncryptedMedia(video, mediaFile, keySystem, key) {
  var keyRequested = false;
  var sourceOpened = false;
  // Add a property to video to check key was added.
  video.hasKeyAdded = false;

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
      setDocTitle("GenerateKeyRequestException");
    }
  }

  function onKeyAdded() {
    video.hasKeyAdded = true;
  }

  function onKeyMessage(e) {
    if (isHeartBeatMessage(e.message)) {
      console.log('onKeyMessage - heart beat', e);
      return;
    }

    console.log('onKeyMessage - key request', e);
    video.webkitAddKey(keySystem, key, e.message);
  }

  var mediaSource = new WebKitMediaSource();

  mediaSource.addEventListener('webkitsourceopen', onSourceOpen);
  video.addEventListener('webkitneedkey', onNeedKey);
  video.addEventListener('webkitkeymessage', onKeyMessage);
  video.addEventListener('webkitkeyerror', failTest);
  video.addEventListener('webkitkeyadded', onKeyAdded);
  installTitleEventHandler(video, 'error');

  video.src = window.URL.createObjectURL(mediaSource);
  return mediaSource;
}
