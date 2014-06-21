// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Does common handling for requests coming from web pages and
 * routes them to the provided handler.
 */

/**
 * Gets the scheme + origin from a web url.
 * @param {string} url Input url
 * @return {?string} Scheme and origin part if url parses
 */
function getOriginFromUrl(url) {
  var re = new RegExp('^(https?://)[^/]*/?');
  var originarray = re.exec(url);
  if (originarray == null) return originarray;
  var origin = originarray[0];
  while (origin.charAt(origin.length - 1) == '/') {
    origin = origin.substring(0, origin.length - 1);
  }
  if (origin == 'http:' || origin == 'https:')
    return null;
  return origin;
}

/**
 * Returns whether the signData object appears to be valid.
 * @param {Array.<Object>} signData the signData object.
 * @return {boolean} whether the object appears valid.
 */
function isValidSignData(signData) {
  for (var i = 0; i < signData.length; i++) {
    var incomingChallenge = signData[i];
    if (!incomingChallenge.hasOwnProperty('challenge'))
      return false;
    if (!incomingChallenge.hasOwnProperty('appId')) {
      return false;
    }
    if (!incomingChallenge.hasOwnProperty('keyHandle'))
      return false;
    if (incomingChallenge['version']) {
      if (incomingChallenge['version'] != 'U2F_V1' &&
          incomingChallenge['version'] != 'U2F_V2') {
        return false;
      }
    }
  }
  return true;
}

/** Posts the log message to the log url.
 * @param {string} logMsg the log message to post.
 * @param {string=} opt_logMsgUrl the url to post log messages to.
 */
function logMessage(logMsg, opt_logMsgUrl) {
  console.log(UTIL_fmt('logMessage("' + logMsg + '")'));

  if (!opt_logMsgUrl) {
    return;
  }
  // Image fetching is not allowed per packaged app CSP.
  // But video and audio is.
  var audio = new Audio();
  audio.src = opt_logMsgUrl + logMsg;
}

/**
 * Formats response parameters as an object.
 * @param {string} type type of the post message.
 * @param {number} code status code of the operation.
 * @param {Object=} responseData the response data of the operation.
 * @return {Object} formatted response.
 */
function formatWebPageResponse(type, code, responseData) {
  var responseJsonObject = {};
  responseJsonObject['type'] = type;
  responseJsonObject['code'] = code;
  if (responseData)
    responseJsonObject['responseData'] = responseData;
  return responseJsonObject;
}

/**
 * @param {!string} string Input string
 * @return {Array.<number>} SHA256 hash value of string.
 */
function sha256HashOfString(string) {
  var s = new SHA256();
  s.update(UTIL_StringToBytes(string));
  return s.digest();
}

/**
 * Normalizes the TLS channel ID value:
 * 1. Converts semantically empty values (undefined, null, 0) to the empty
 *     string.
 * 2. Converts valid JSON strings to a JS object.
 * 3. Otherwise, returns the input value unmodified.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel id
 * @return {Object|string} The normalized TLS channel ID value.
 */
function tlsChannelIdValue(opt_tlsChannelId) {
  if (!opt_tlsChannelId) {
    // Case 1: Always set some value for  TLS channel ID, even if it's the empty
    // string: this browser definitely supports them.
    return '';
  }
  if (typeof opt_tlsChannelId === 'string') {
    try {
      var obj = JSON.parse(opt_tlsChannelId);
      if (!obj) {
        // Case 1: The string value 'null' parses as the Javascript object null,
        // so return an empty string: the browser definitely supports TLS
        // channel id.
        return '';
      }
      // Case 2: return the value as a JS object.
      return /** @type {Object} */ (obj);
    } catch (e) {
      console.warn('Unparseable TLS channel ID value ' + opt_tlsChannelId);
      // Case 3: return the value unmodified.
    }
  }
  return opt_tlsChannelId;
}

/**
 * Creates a browser data object with the given values.
 * @param {!string} type A string representing the "type" of this browser data
 *     object.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeBrowserData(type, serverChallenge, origin, opt_tlsChannelId) {
  var browserData = {
    'typ' : type,
    'challenge' : serverChallenge,
    'origin' : origin
  };
  browserData['cid_pubkey'] = tlsChannelIdValue(opt_tlsChannelId);
  return JSON.stringify(browserData);
}

/**
 * Creates a browser data object for an enroll request with the given values.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeEnrollBrowserData(serverChallenge, origin, opt_tlsChannelId) {
  return makeBrowserData(
      'navigator.id.finishEnrollment', serverChallenge, origin,
      opt_tlsChannelId);
}

/**
 * Creates a browser data object for a sign request with the given values.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeSignBrowserData(serverChallenge, origin, opt_tlsChannelId) {
  return makeBrowserData(
      'navigator.id.getAssertion', serverChallenge, origin, opt_tlsChannelId);
}

/**
 * @param {string} browserData Browser data as JSON
 * @param {string} appId Application Id
 * @param {string} encodedKeyHandle B64 encoded key handle
 * @param {string=} version Protocol version
 * @return {SignHelperChallenge} Challenge object
 */
function makeChallenge(browserData, appId, encodedKeyHandle, version) {
  var appIdHash = B64_encode(sha256HashOfString(appId));
  var browserDataHash = B64_encode(sha256HashOfString(browserData));
  var keyHandle = encodedKeyHandle;

  var challenge = {
    'challengeHash': browserDataHash,
    'appIdHash': appIdHash,
    'keyHandle': keyHandle
  };
  // Version is implicitly U2F_V1 if not specified.
  challenge['version'] = (version || 'U2F_V1');
  return challenge;
}

/**
 * Makes a sign helper request from an array of challenges.
 * @param {Array.<SignHelperChallenge>} challenges The sign challenges.
 * @param {number=} opt_timeoutSeconds Timeout value.
 * @param {string=} opt_logMsgUrl URL to log to.
 * @return {SignHelperRequest} The sign helper request.
 */
function makeSignHelperRequest(challenges, opt_timeoutSeconds, opt_logMsgUrl) {
  var request = {
    'type': 'sign_helper_request',
    'signData': challenges,
    'timeout': opt_timeoutSeconds || 0
  };
  if (opt_logMsgUrl !== undefined) {
    request.logMsgUrl = opt_logMsgUrl;
  }
  return request;
}
