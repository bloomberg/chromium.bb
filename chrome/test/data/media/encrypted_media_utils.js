// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var keySystem = QueryString.keySystem;
var mediaFile = QueryString.mediaFile;
var mediaType = QueryString.mediaType || 'video/webm; codecs="vorbis, vp8"';
var useMSE = QueryString.useMSE == 1;
var usePrefixedEME = QueryString.usePrefixedEME == 1;
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

// JWK routines copied from third_party/WebKit/LayoutTests/media/
//   encrypted-media/encrypted-media-utils.js
//
// Encodes data (Uint8Array) into base64 string without trailing '='.
// TODO(jrummell): Update once the EME spec is updated to say base64url
// encoding.
function base64Encode(data) {
  var result = btoa(String.fromCharCode.apply(null, data));
  return result.replace(/=+$/g, '');
}

// Creates a JWK from raw key ID and key.
function createJWK(keyId, key) {
  var jwk = "{\"kty\":\"oct\",\"kid\":\"";
  jwk += base64Encode(keyId);
  jwk += "\",\"k\":\"";
  jwk += base64Encode(key);
  jwk += "\"}";
  return jwk;
}

// Creates a JWK Set from an array of JWK(s).
function createJWKSet() {
  var jwkSet = "{\"keys\":[";
  for (var i = 0; i < arguments.length; i++) {
    if (i != 0)
      jwkSet += ",";
    jwkSet += arguments[i];
  }
  jwkSet += "]}";
  return jwkSet;
}

function isHeartbeatMessage(msg) {
  return hasPrefix(msg, HEART_BEAT_HEADER);
}

function isFileIOTestMessage(msg) {
  return hasPrefix(msg, FILE_IO_TEST_RESULT_HEADER);
}

function loadEncryptedMediaFromURL(video) {
  return loadEncryptedMedia(video, mediaFile, keySystem, KEY, useMSE,
                            usePrefixedEME);
}

function loadEncryptedMedia(video, mediaFile, keySystem, key, useMSE,
                            usePrefixedEME, appendSourceCallbackFn) {
  var keyRequested = false;
  var sourceOpened = false;
  var mediaKeys;
  var mediaKeySession;
  // Add properties to enable verification that events occurred.
  video.receivedKeyAdded = false;
  video.receivedHeartbeat = false;
  video.isHeartbeatExpected = keySystem === EXTERNAL_CLEAR_KEY_KEY_SYSTEM;
  video.receivedKeyMessage = false;

  if (!(video && mediaFile && keySystem && key)) {
    failTest('Missing parameters in loadEncryptedMedia().');
    return;
  }

  // Shared by prefixed and unprefixed EME.
  function onNeedKey(e) {
    if (keyRequested)
      return;
    keyRequested = true;
    console.log('onNeedKey', e);

    var initData = sessionToLoad ? stringToUint8Array(
        PREFIXED_API_LOAD_SESSION_HEADER + sessionToLoad) : e.initData;
    try {
      if (usePrefixedEME) {
        video.webkitGenerateKeyRequest(keySystem, initData);
      } else {
        mediaKeySession = mediaKeys.createSession(e.contentType, initData);
        mediaKeySession.addEventListener('message', onMessage);
        mediaKeySession.addEventListener('error', function() {
            setResultInTitle("KeyError");
        });
        mediaKeySession.addEventListener('ready', onReady);
      }
    }
    catch(error) {
      setResultInTitle(error.name);
    }
  }

  // Prefixed EME callbacks.
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

    processMessage(message, message.keySystem, message.defaultURL);
  }

  // Unprefixed EME callbacks.
  function onReady(e) {
  }

  function onMessage(message) {
    processMessage(message, keySystem, message.destinationURL);
  }

  // Shared by prefixed and unprefixed EME.
  function processMessage(message, keySystem, url) {
    video.receivedKeyMessage = true;

    if (!message.message) {
      failTest('Message without a message content: ' + message.message);
      return;
    }

    if (isHeartbeatMessage(message.message)) {
      console.log('processMessage - heartbeat', message);
      video.receivedHeartbeat = true;
      verifyHeartbeatMessage(keySystem, url);
      return;
    }

    if (isFileIOTestMessage(message.message)) {
      var success = getFileIOTestResult(keySystem, message);
      console.log('processMessage - CDM file IO test: ' +
                  (success ? 'Success' : 'Fail'));
      if (success)
        setResultInTitle("FILEIOTESTSUCCESS");
      else
        setResultInTitle("FAILED");
      return;
    }

    // For FileIOTest key system, no need to start playback.
    if (keySystem == EXTERNAL_CLEAR_KEY_FILE_IO_TEST_KEY_SYSTEM)
      return;

    // No tested key system returns defaultURL in for key request messages.
    if (url) {
      failTest('Message unexpectedly has URL: ' + url);
      return;
    }

    // When loading a session, no need to call update()/webkitAddKey().
    if (sessionToLoad)
      return;

    console.log('processMessage - key request', message);
    if (forceInvalidResponse) {
      console.log('processMessage - Forcing an invalid response.');
      var invalidData = new Uint8Array([0xAA]);
      if (usePrefixedEME) {
        video.webkitAddKey(keySystem, invalidData, invalidData);
      } else {
        mediaKeySession.update(invalidData);
      }
      return;
    }
    // Check if should send request to locally running license server.
    if (licenseServerURL) {
      requestLicense(message);
      return;
    }
    console.log('processMessage - Respond with test key.');
    var initData = message.message;
    if (mediaType.indexOf('mp4') != -1)
      initData = KEY_ID; // Temporary hack for Clear Key in v0.1.
    if (usePrefixedEME) {
      video.webkitAddKey(keySystem, key, initData);
    } else {
      var jwkSet = stringToUint8Array(createJWKSet(createJWK(initData, key)));
      mediaKeySession.update(jwkSet);
    }
  }

  function verifyHeartbeatMessage(keySystem, url) {
    String.prototype.startsWith = function(prefix) {
      return this.indexOf(prefix) === 0;
    }

    function isExternalClearKey(keySystem) {
      return keySystem == EXTERNAL_CLEAR_KEY_KEY_SYSTEM ||
             keySystem.startsWith(EXTERNAL_CLEAR_KEY_KEY_SYSTEM + '.');
    }

    // Only External Clear Key sends a HEARTBEAT message.
    if (!isExternalClearKey(keySystem)) {
      failTest('Unexpected heartbeat from ' + keySystem);
      return;
    }

    if (url != EXTERNAL_CLEAR_KEY_HEARTBEAT_URL) {
      failTest('Heartbeat message with unexpected URL: ' + url);
      return;
    }
  }

  function getFileIOTestResult(keySystem, e) {
    // Only External Clear Key sends a FILEIOTESTRESULT message.
    if (keySystem != EXTERNAL_CLEAR_KEY_FILE_IO_TEST_KEY_SYSTEM) {
      failTest('Unexpected CDM file IO test result from ' + keySystem);
      return false;
    }

    // The test result is either '0' or '1' appended to the header.
    if (e.message.length != FILE_IO_TEST_RESULT_HEADER.length + 1)
      return false;

    var result_index = FILE_IO_TEST_RESULT_HEADER.length;
    return String.fromCharCode(e.message[result_index]) == 1;
  }

  if (usePrefixedEME) {
    video.addEventListener('webkitneedkey', onNeedKey);
    video.addEventListener('webkitkeymessage', onKeyMessage);
    video.addEventListener('webkitkeyerror', function() {
        setResultInTitle("KeyError");
    });
    video.addEventListener('webkitkeyadded', onKeyAdded);
  } else {
    video.addEventListener('needkey', onNeedKey);
  }
  installTitleEventHandler(video, 'error');

  if (useMSE) {
    var mediaSource = loadMediaSource(mediaFile, mediaType,
                                      appendSourceCallbackFn);
    video.src = window.URL.createObjectURL(mediaSource);
  } else {
    video.src = mediaFile;
  }
  if (!usePrefixedEME) {
    mediaKeys = new MediaKeys(keySystem);
    video.setMediaKeys(mediaKeys);
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
