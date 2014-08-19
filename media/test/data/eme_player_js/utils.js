// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utils provide logging functions and other JS functions commonly used by the
// app and media players.
var Utils = new function() {
  this.titleChanged = false;
};

// Adds options to document element.
Utils.addOptions = function(elementID, keyValueOptions, disabledOptions) {
  disabledOptions = disabledOptions || [];
  var selectElement = document.getElementById(elementID);
  var keys = Object.keys(keyValueOptions);
  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    var option = new Option(key, keyValueOptions[key]);
    option.title = keyValueOptions[key];
    if (disabledOptions.indexOf(key) >= 0)
      option.disabled = true;
    selectElement.options.add(option);
  }
};

Utils.convertToArray = function(input) {
  if (Array.isArray(input))
    return input;
  return [input];
};

Utils.convertToUint8Array = function(msg) {
  if (typeof msg == 'string') {
    var ans = new Uint8Array(msg.length);
    for (var i = 0; i < msg.length; i++) {
      ans[i] = msg.charCodeAt(i);
    }
    return ans;
  }
  // Assume it is an ArrayBuffer or ArrayBufferView. If it already is a
  // Uint8Array, this will just make a copy of the view.
  return new Uint8Array(msg);
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

Utils.extractFirstLicenseKey = function(message) {
  // Decodes data (Uint8Array) from base64 string.
  // TODO(jrummell): Update once the EME spec is updated to say base64url
  // encoding.
  function base64Decode(data) {
    return atob(data);
  }

  function convertToString(data) {
    return String.fromCharCode.apply(null, Utils.convertToUint8Array(data));
  }

  try {
    var json = JSON.parse(convertToString(message));
    // Decode the first element of 'kids', return it as an Uint8Array.
    return Utils.convertToUint8Array(base64Decode(json.kids[0]));
  } catch (error) {
    // Not valid JSON, so return message untouched as Uint8Array.
    return Utils.convertToUint8Array(message);
  }
}

Utils.documentLog = function(log, success, time) {
  if (!docLogs)
    return;
  time = time || Utils.getCurrentTimeString();
  var timeLog = '<span style="color: green">' + time + '</span>';
  var logColor = !success ? 'red' : 'black'; // default is true.
  log = '<span style="color: "' + logColor + '>' + log + '</span>';
  docLogs.innerHTML = timeLog + ' - ' + log + '<br>' + docLogs.innerHTML;
};

Utils.ensureOptionInList = function(listID, option) {
  var selectElement = document.getElementById(listID);
  for (var i = 0; i < selectElement.length; i++) {
    if (selectElement.options[i].value == option) {
      selectElement.value = option;
      return;
    }
  }
  // The list does not have the option, let's add it and select it.
  var optionElement = new Option(option, option);
  optionElement.title = option;
  selectElement.options.add(optionElement);
  selectElement.value = option;
};

Utils.failTest = function(msg, newTitle) {
  var failMessage = 'FAIL: ';
  var title = 'FAILED';
  // Handle exception messages;
  if (msg.message) {
    title = msg.name || 'Error';
    failMessage += title + ' ' + msg.message;
  } else if (msg instanceof Event) {
    // Handle failing events.
    failMessage = msg.target + '.' + msg.type;
    title = msg.type;
  } else {
    failMessage += msg;
  }
  // Force newTitle if passed.
  title = newTitle || title;
  // Log failure.
  Utils.documentLog(failMessage, false);
  console.log(failMessage, msg);
  Utils.setResultInTitle(title);
};

Utils.getCurrentTimeString = function() {
  var date = new Date();
  var hours = ('0' + date.getHours()).slice(-2);
  var minutes = ('0' + date.getMinutes()).slice(-2);
  var secs = ('0' + date.getSeconds()).slice(-2);
  var milliSecs = ('00' + date.getMilliseconds()).slice(-3);
  return hours + ':' + minutes + ':' + secs + '.' + milliSecs;
};

Utils.getDefaultKey = function(forceInvalidResponse) {
  if (forceInvalidResponse) {
    Utils.timeLog('Forcing invalid key data.');
    return new Uint8Array([0xAA]);
  }
  return KEY;
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

Utils.getInitDataFromMessage = function(message, mediaType, decodeJSONMessage) {
  var initData;
  if (mediaType.indexOf('mp4') != -1) {
    // Temporary hack for Clear Key in v0.1.
    // If content uses mp4, then message.message is PSSH data. Instead of
    // parsing that data we hard code the initData.
    initData = Utils.convertToUint8Array(KEY_ID);
  } else if (decodeJSONMessage) {
    initData = Utils.extractFirstLicenseKey(message.message);
  } else {
    initData = Utils.convertToUint8Array(message.message);
  }
  return initData;
};

Utils.hasPrefix = function(msg, prefix) {
  var message = String.fromCharCode.apply(null, msg);
  return message.substring(0, prefix.length) == prefix;
};

Utils.installTitleEventHandler = function(element, event) {
  element.addEventListener(event, function(e) {
    Utils.setResultInTitle(e.type);
  }, false);
};

Utils.isHeartBeatMessage = function(msg) {
  return Utils.hasPrefix(Utils.convertToUint8Array(msg), HEART_BEAT_HEADER);
};

Utils.resetTitleChange = function() {
  this.titleChanged = false;
  document.title = '';
};

Utils.sendRequest = function(requestType, responseType, message, serverURL,
                             onSuccessCallbackFn, forceInvalidResponse) {
  var requestAttemptCount = 0;
  var MAXIMUM_REQUEST_ATTEMPTS = 3;
  var REQUEST_RETRY_DELAY_MS = 3000;

  function sendRequestAttempt() {
    requestAttemptCount++;
    if (requestAttemptCount == MAXIMUM_REQUEST_ATTEMPTS) {
      Utils.failTest('FAILED: Exceeded maximum license request attempts.');
      return;
    }
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
        Utils.timeLog('Retrying request if possible in ' +
                      REQUEST_RETRY_DELAY_MS + 'ms');
        setTimeout(sendRequestAttempt, REQUEST_RETRY_DELAY_MS);
      }
    };
    Utils.timeLog('Attempt (' + requestAttemptCount +
                  '): sending request to server: ' + serverURL);
    xmlhttp.send(message);
  }

  if (forceInvalidResponse) {
    Utils.timeLog('Not sending request - forcing an invalid response.');
    return onSuccessCallbackFn([0xAA]);
  }
  sendRequestAttempt();
};

Utils.setResultInTitle = function(title) {
  // If document title is 'ENDED', then update it with new title to possibly
  // mark a test as failure.  Otherwise, keep the first title change in place.
  if (!this.titleChanged || document.title.toUpperCase() == 'ENDED')
    document.title = title.toUpperCase();
  Utils.timeLog('Set document title to: ' + title + ', updated title: ' +
                document.title);
  this.titleChanged = true;
};

Utils.timeLog = function(/**/) {
  if (arguments.length == 0)
    return;
  var time = Utils.getCurrentTimeString();
  // Log to document.
  Utils.documentLog(arguments[0], time);
  // Log to JS console.
  var logString = time + ' - ';
  for (var i = 0; i < arguments.length; i++) {
    logString += ' ' + arguments[i];
  }
  console.log(logString);
};
