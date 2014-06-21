// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview U2F gnubbyd background page
 */

'use strict';

// Singleton tracking available devices.
var gnubbies = new Gnubbies();
llUsbGnubby.register(gnubbies);
// Only include HID support if it's available in this browser.
if (chrome.hid) {
  llHidGnubby.register(gnubbies);
}

var GNUBBY_FACTORY = new UsbGnubbyFactory(gnubbies);
var TIMER_FACTORY = new CountdownTimerFactory();
var SIGN_HELPER_FACTORY =
    new UsbSignHelperFactory(GNUBBY_FACTORY, TIMER_FACTORY);
var ENROLL_HELPER_FACTORY =
    new UsbEnrollHelperFactory(GNUBBY_FACTORY, TIMER_FACTORY);

/**
 * Response status codes
 * @const
 * @enum {number}
 */
var ErrorCodes = {
  'OK': 0,
  'OTHER_ERROR': 1,
  'BAD_REQUEST': 2,
  'CONFIGURATION_UNSUPPORTED': 3,
  'DEVICE_INELIGIBLE': 4,
  'TIMEOUT': 5
};

/**
 * Message types for messsages to/from the extension
 * @const
 * @enum {string}
 */
var MessageTypes = {
  U2F_REGISTER_REQUEST: 'u2f_register_request',
  U2F_SIGN_REQUEST: 'u2f_sign_request',
  U2F_REGISTER_RESPONSE: 'u2f_register_response',
  U2F_SIGN_RESPONSE: 'u2f_sign_response'
};

/**
 * Translates a U2F request into a Gnubbyd web request.
 * @param {Object} request U2F Request to translate
 * @return {Object} The old (gnubbyd) style web request
 * @private
 */
function translateFromU2FRequest_(request) {
  var newRequest = {};
  if (request['type'] == MessageTypes.U2F_REGISTER_REQUEST) {
    newRequest['type'] = GnubbyMsgTypes.ENROLL_WEB_REQUEST;
  } else if (request['type'] == MessageTypes.U2F_SIGN_REQUEST) {
    newRequest['type'] = GnubbyMsgTypes.SIGN_WEB_REQUEST;
  } else {
    return null;
  }

  if (request['signRequests']) {
    newRequest['signData'] = request['signRequests'];
  }

  if (request['registerRequests']) {
    newRequest['enrollChallenges'] = request['registerRequests'];
  }

  if (typeof request['timeoutSeconds'] !== 'undefined')
    newRequest['requestId'] = request['requestId'];

  // requestId is handled by the message handlers below

  return newRequest;
}

/**
 * Translates a Gnubbyd response to a U2F response
 * @param {Object} response The old (gnubbyd) style web response
 * @return {Object} The corresponding U2F response
 * @private
 */
function translateToU2FResponse_(response) {
  var newResponse = {};
  if (response['type'] == GnubbyMsgTypes.ENROLL_WEB_REPLY) {
    newResponse['type'] = MessageTypes.U2F_REGISTER_RESPONSE;
  } else if (response['type'] == GnubbyMsgTypes.SIGN_WEB_REPLY) {
    newResponse['type'] = MessageTypes.U2F_SIGN_RESPONSE;
  } else {
    return null;
  }

  if (response['code'] == GnubbyCodeTypes.OK) {
    var gnubbyResponseData = response['responseData'];
    var u2fResponseData = {};
    u2fResponseData['clientData'] = gnubbyResponseData['browserData'];
    if (gnubbyResponseData['keyHandle'])
      u2fResponseData['keyHandle'] = gnubbyResponseData['keyHandle'];
    if (gnubbyResponseData['signatureData'])
      u2fResponseData['signatureData'] = gnubbyResponseData['signatureData'];
    if (gnubbyResponseData['enrollData'])
      u2fResponseData['registrationData'] = gnubbyResponseData['enrollData'];
    newResponse['responseData'] = u2fResponseData;
  } else {
    var code;
    switch (response['code']) {
    case GnubbyCodeTypes.BAD_REQUEST:
      code = ErrorCodes.BAD_REQUEST;
      break;

    case GnubbyCodeTypes.ALREADY_ENROLLED:
    case GnubbyCodeTypes.NONE_PLUGGED_ENROLLED:
    case GnubbyCodeTypes.NO_DEVICES_ENROLLED:
      code = ErrorCodes.DEVICE_INELIGIBLE;
      break;

    case GnubbyCodeTypes.BAD_APP_ID:
      console.error('Invalid appId');
      code = ErrorCodes.BAD_REQUEST;
      break;

    case GnubbyCodeTypes.UNKNOWN_ERROR:
      code = ErrorCodes.OTHER_ERROR;
      break;

    case GnubbyCodeTypes.WAIT_TOUCH:
      code = ErrorCodes.TIMEOUT;
      break;

    case GnubbyCodeTypes.NO_GNUBBIES:
    case GnubbyCodeTypes.BUSY:
      console.warn('Ignoring a deprecated gnubbyd error', response);
      return null;
    }

    var error = {'errorCode': code};
    if (response['errorDetail'])
      error['errorMessage'] = response['errorDetail'];
    newResponse['responseData'] = error;
  }

  // requestId is handled by the message handlers below

  return newResponse;
}

/**
 * @param {Object} request Request object
 * @param {MessageSender} sender Sender frame
 * @param {Function} sendResponse Response callback
 * @return {?Closeable} Optional handler object that should be closed when port
 *     closes
 */
function handleWebPageRequest(request, sender, sendResponse) {

  var wrappedSendResponse = function(resp) {
    console.log(JSON.stringify(resp));
    var u2fResponse = translateToU2FResponse_(resp);
    if (!u2fResponse) {
      console.log('Dropping gnubby response which is not compatible with U2F',
          resp);
      return;
    }
    sendResponse(u2fResponse);
  };

  switch (request.type) {
    case MessageTypes.U2F_REGISTER_REQUEST:
      return handleEnrollRequest(ENROLL_HELPER_FACTORY, sender,
          translateFromU2FRequest_(request), wrappedSendResponse);

    case GnubbyMsgTypes.ENROLL_WEB_REQUEST:
      return handleEnrollRequest(ENROLL_HELPER_FACTORY, sender,
          request, sendResponse);

    case MessageTypes.U2F_SIGN_REQUEST:
      return handleSignRequest(SIGN_HELPER_FACTORY, sender,
          translateFromU2FRequest_(request), wrappedSendResponse);

    case GnubbyMsgTypes.SIGN_WEB_REQUEST:
      return handleSignRequest(SIGN_HELPER_FACTORY, sender,
          request, sendResponse);

    default:
      var response = {
        'type': MessageTypes.U2F_REGISTER_RESPONSE,
        'error': { 'errorCode': ErrorCodes.BAD_REQUEST }
      };
      sendResponse(response);
      return null;
  }
}

// Listen to individual messages sent from (whitelisted) webpages via
// chrome.runtime.sendMessage
function messageHandlerExternal(request, sender, sendResponse) {
  var closeable = handleWebPageRequest(request, sender, function(response) {
    response['requestId'] = request['requestId'];
    sendResponse(response);
    if (closeable) {
      closeable.close();
    }
  });
}
chrome.runtime.onMessageExternal.addListener(messageHandlerExternal);

// Listen to direct connection events, and wire up a message handler on the port
chrome.runtime.onConnectExternal.addListener(function(port) {
  var closeable;
  port.onMessage.addListener(function(request) {
    closeable = handleWebPageRequest(request, port.sender,
        function(response) {
          response['requestId'] = request['requestId'];
          port.postMessage(response);
        });
  });
  port.onDisconnect.addListener(function() {
    if (closeable) {
      closeable.close();
    }
  });
});
