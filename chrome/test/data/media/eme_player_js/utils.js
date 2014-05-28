// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utils provide logging functions and other JS functions commonly used by the
// app and media players.
var Utils = new function() {
}

Utils.isHeartBeatMessage = function(msg) {
  var message = String.fromCharCode.apply(null, msg);
  return message.substring(0, HEART_BEAT_HEADER.length) == HEART_BEAT_HEADER;
};

Utils.convertToArray = function(input) {
  if (Array.isArray(input))
    return input;
  return [input];
};

Utils.getHexString = function(uintArray) {
  var hex_str = '';
  for (var i = 0; i < uintArray.length; i++) {
    var hex = uintArray[i].toString(16);
    if (hex.length == 1)
      hex = '0' + hex;
    hex_str += hex;
  }
  return hex_str;
};

Utils.convertToUint8Array = function(msg) {
  var ans = new Uint8Array(msg.length);
  for (var i = 0; i < msg.length; i++) {
    ans[i] = msg.charCodeAt(i);
  }
  return ans;
};

Utils.timeLog = function(/**/) {
  function documentLog(time, log) {
    var time = '<span style="color: green">' + time + '</span>';
    docLogs.innerHTML = time + ' - ' + log + '<br>' + docLogs.innerHTML;
  }

  if (arguments.length == 0)
    return;
  var time = Utils.getCurrentTimeString();
  // Log to document.
  documentLog(time, arguments[0]);

  // Log to JS console.
  var args = [];
  args.push(time + ' - ');
  for (var i = 0; i < arguments.length; i++) {
    args.push(arguments[i]);
  }
  console.log.apply(console, args);
};

Utils.getCurrentTimeString = function() {
  var date = new Date();
  var hours = ('0' + date.getHours()).slice(-2);
  var minutes = ('0' + date.getMinutes()).slice(-2);
  var secs = ('0' + date.getSeconds()).slice(-2);
  var milliSecs = ('00' + date.getMilliseconds()).slice(-3);
  return hours + ':' + minutes + ':' + secs + '.' + milliSecs;
};

Utils.failTest = function(msg) {
  var failMessage = 'FAIL: ';
  // Handle exception messages;
  if (msg.message) {
    failMessage += msg.name ? msg.name : 'Error ';
    failMessage += msg.message;
  }
  // Handle failing events.
  else if (msg instanceof Event)
    failMessage = msg.target + '.' + msg.type;
  else
    failMessage = msg;
  Utils.timeLog('<span style="color: red">' + failMessage + '</span>', msg);
};

Utils.sendRequest = function(requestType, responseType, message, serverURL,
                             onSuccessCallbackFn) {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.responseType = responseType;
  xmlhttp.open(requestType, serverURL, true);

  xmlhttp.onload = function(e) {
    if (this.status == 200) {
      if (onSuccessCallbackFn)
        onSuccessCallbackFn(this.response);
    } else {
      Utils.timeLog('Bad response status: ' + this.status);
      Utils.timeLog('Bad response: ' + this.response);
    }
  };
  Utils.timeLog('Sending request to server: ' + serverURL);
  xmlhttp.send(message);
};

// Adds options to document element.
Utils.addOptions = function(elementID, keyValueOptions) {
  var selectElement = document.getElementById(elementID);
  var keys = Object.keys(keyValueOptions);
  for (var i = 0; i < keys.length; i++) {
    var option = new Option(keys[i], keyValueOptions[keys[i]]);
    option.title = keyValueOptions[keys[i]];
    selectElement.options.add(option);
  }
};

Utils.createJWKData = function(keyId, key) {
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
    var jwk = '{"kty":"oct","kid":"';
    jwk += base64Encode(keyId);
    jwk += '","k":"';
    jwk += base64Encode(key);
    jwk += '"}';
    return jwk;
  }

  // Creates a JWK Set from an array of JWK(s).
  function createJWKSet() {
    var jwkSet = '{"keys":[';
    for (var i = 0; i < arguments.length; i++) {
      if (i != 0)
        jwkSet += ',';
      jwkSet += arguments[i];
    }
    jwkSet += ']}';
    return jwkSet;
  }

  return Utils.convertToUint8Array(createJWKSet(createJWK(keyId, key)));
};
