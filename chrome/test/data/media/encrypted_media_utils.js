// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var keySystem = QueryString.keySystem;
var mediaFile = QueryString.mediaFile;
var mediaType = QueryString.mediaType || 'video/webm; codecs="vorbis, vp8"';
var useMSE = QueryString.useMSE == 1;
var forceInvalidResponse = QueryString.forceInvalidResponse == 1;
var sessionToLoad = QueryString.sessionToLoad;
var licenseServerURL = QueryString.licenseServerURL;
// Maximum license request attempts that the media element can make to get a
// valid license response from the license server.
// This is used to avoid server boot up delays since there is no direct way
// to know if it is ready crbug.com/339289.
var MAX_LICENSE_REQUEST_ATTEMPTS = 3;
// Delay in ms between retries to get a license from license server.
var LICENSE_REQUEST_RETRY_DELAY_MS = 3000;

// Default key used to encrypt many media files used in browser tests.
var KEY = new Uint8Array([0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                          0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c]);
// KEY_ID constant used as init data while encrypting test media files.
var KEY_ID = getInitDataFromKeyId("0123456789012345");
// Heart beat message header.
var HEART_BEAT_HEADER = 'HEARTBEAT';
var FILE_IO_TEST_RESULT_HEADER = 'FILEIOTESTRESULT';
var PREFIXED_API_LOAD_SESSION_HEADER = "LOAD_SESSION|";
var EXTERNAL_CLEAR_KEY_KEY_SYSTEM = "org.chromium.externalclearkey";
var EXTERNAL_CLEAR_KEY_FILE_IO_TEST_KEY_SYSTEM =
    "org.chromium.externalclearkey.fileiotest";
// Note that his URL has been normalized from the one in clear_key_cdm.cc.
var EXTERNAL_CLEAR_KEY_HEARTBEAT_URL =
    'http://test.externalclearkey.chromium.org/';

function stringToUint8Array(str) {
    var result = new Uint8Array(str.length);
    for(var i = 0; i < str.length; i++) {
        result[i] = str.charCodeAt(i);
    }
    return result;
}

function hasPrefix(msg, prefix) {
  if (msg.length < prefix.length)
    return false;
  for (var i = 0; i < prefix.length; ++i) {
    if (String.fromCharCode(msg[i]) != prefix[i])
      return false;
  }
  return true;
}

function isHeartbeatMessage(msg) {
  return hasPrefix(msg, HEART_BEAT_HEADER);
}

function isFileIOTestMessage(msg) {
  return hasPrefix(msg, FILE_IO_TEST_RESULT_HEADER);
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

    var initData = sessionToLoad ? stringToUint8Array(
        PREFIXED_API_LOAD_SESSION_HEADER + sessionToLoad) : e.initData;
    try {
      video.webkitGenerateKeyRequest(keySystem, initData);
    }
    catch(error) {
      setResultInTitle(error.name);
    }
  }

  function onKeyAdded(e) {
    e.target.receivedKeyAdded = true;
  }

  function onKeyMessage(message) {
    video.receivedKeyMessage = true;
    if (!message.keySystem || message.keySystem != keySystem) {
      failTest('Message with unexpected keySystem: ' + message.keySystem);
      return;
    }

    if (!message.sessionId) {
      failTest('Message without a sessionId: ' + message.sessionId);
      return;
    }

    if (!message.message) {
      failTest('Message without a message content: ' + message.message);
      return;
    }

    if (isHeartbeatMessage(message.message)) {
      console.log('onKeyMessage - heartbeat', message);
      message.target.receivedHeartbeat = true;
      verifyHeartbeatMessage(message);
      return;
    }

    if (isFileIOTestMessage(message.message)) {
      var success = getFileIOTestResult(message);
      console.log('onKeyMessage - CDM file IO test: ' +
                  (success ? 'Success' : 'Fail'));
      if (success)
        setResultInTitle("FILEIOTESTSUCCESS");
      else
        setResultInTitle("FAILED");
      return;
    }

    // For FileIOTest key system, no need to start playback.
    if (message.keySystem == EXTERNAL_CLEAR_KEY_FILE_IO_TEST_KEY_SYSTEM)
      return;

    // No tested key system returns defaultURL in for key request messages.
    if (message.defaultURL) {
      failTest('Message unexpectedly has defaultURL: ' + message.defaultURL);
      return;
    }

    // When loading a session, no need to call webkitAddKey().
    if (sessionToLoad)
      return;

    console.log('onKeyMessage - key request', message);
    if (forceInvalidResponse) {
      console.log('Forcing an invalid onKeyMessage response.');
      var invalidData = new Uint8Array([0xAA]);
      video.webkitAddKey(keySystem, invalidData, invalidData);
      return;
    }
    // Check if should send request to locally running license server.
    if (licenseServerURL) {
      requestLicense(message);
      return;
    }
    console.log('Respond to onKeyMessage with test key.');
    var initData = message.message;
    if (mediaType.indexOf('mp4') != -1)
      initData = KEY_ID; // Temporary hack for Clear Key in v0.1.
    video.webkitAddKey(keySystem, key, initData);
  }

  function verifyHeartbeatMessage(message) {
    String.prototype.startsWith = function(prefix) {
      return this.indexOf(prefix) === 0;
    }

    function isExternalClearKey(keySystem) {
      return keySystem == EXTERNAL_CLEAR_KEY_KEY_SYSTEM ||
             keySystem.startsWith(EXTERNAL_CLEAR_KEY_KEY_SYSTEM + '.');
    }

    // Only External Clear Key sends a HEARTBEAT message.
    if (!isExternalClearKey(message.keySystem)) {
      failTest('Unexpected heartbeat from ' + message.keySystem);
      return;
    }

    if (message.defaultURL != EXTERNAL_CLEAR_KEY_HEARTBEAT_URL) {
      failTest('Heartbeat message with unexpected defaultURL: ' +
               message.defaultURL);
      return;
    }
  }

  function getFileIOTestResult(e) {
    // Only External Clear Key sends a FILEIOTESTRESULT message.
    if (e.keySystem != EXTERNAL_CLEAR_KEY_FILE_IO_TEST_KEY_SYSTEM) {
      failTest('Unexpected CDM file IO test result from ' + e.keySystem);
      return false;
    }

    // The test result is either '0' or '1' appended to the header.
    if (e.message.length != FILE_IO_TEST_RESULT_HEADER.length + 1)
      return false;

    var result_index = FILE_IO_TEST_RESULT_HEADER.length;
    return String.fromCharCode(e.message[result_index]) == 1;
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

function requestLicense(message) {
  // Store license request attempts per target <video>.
  if (message.target.licenseRequestAttempts == undefined)
    message.target.licenseRequestAttempts = 0;

  if (message.target.licenseRequestAttempts ==  MAX_LICENSE_REQUEST_ATTEMPTS) {
    failTest('Exceeded maximum license request attempts.');
    return;
  }
  message.target.licenseRequestAttempts++;
  console.log('Requesting license from license server ' + licenseServerURL);
  if (!URLExists(licenseServerURL)) {
    console.log('License server is not available, retrying in ' +
                LICENSE_REQUEST_RETRY_DELAY_MS + ' ms.');
    setTimeout(requestLicense, LICENSE_REQUEST_RETRY_DELAY_MS, message);
    return;
  }

  requestLicenseTry(message);
}

function requestLicenseTry(message) {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.responseType = 'arraybuffer';
  xmlhttp.open("POST", licenseServerURL, true);

  xmlhttp.onload = function(e) {
    if (this.status == 200) {
      var response = new Uint8Array(this.response);
      console.log('Adding license response', response);
      message.target.webkitAddKey(keySystem, response, new Uint8Array(1),
                                  message.sessionId);
      // Reset license request count so that renewal requests can be sent later.
      message.target.licenseRequestAttempts = 0;
    } else {
      console.log('Bad response: ' + this.response);
      console.log('License response bad status = ' + this.status);
      console.log('Retrying license request if possible.');
      setTimeout(requestLicense, LICENSE_REQUEST_RETRY_DELAY_MS, message);
    }
  }
  console.log('license request message', message.message);
  xmlhttp.send(message.message);
}

function URLExists(url) {
  if (!url)
    return false;
  var http = new XMLHttpRequest();
  http.open('HEAD', url, false);
  try {
    http.send();
    return http.status != 404;
  } catch (e) {
    return false;
  }
}
