// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles web page requests for gnubby sign requests.
 */

'use strict';

var signRequestQueue = new OriginKeyedRequestQueue();

/**
 * Handles a sign request.
 * @param {!SignHelperFactory} factory Factory to create a sign helper.
 * @param {MessageSender} sender The sender of the message.
 * @param {Object} request The web page's sign request.
 * @param {Function} sendResponse Called back with the result of the sign.
 * @return {Closeable} Request handler that should be closed when the browser
 *     message channel is closed.
 */
function handleSignRequest(factory, sender, request, sendResponse) {
  var sentResponse = false;
  function sendResponseOnce(r) {
    if (queuedSignRequest) {
      queuedSignRequest.close();
      queuedSignRequest = null;
    }
    if (!sentResponse) {
      sentResponse = true;
      try {
        // If the page has gone away or the connection has otherwise gone,
        // sendResponse fails.
        sendResponse(r);
      } catch (exception) {
        console.warn('sendResponse failed: ' + exception);
      }
    } else {
      console.warn(UTIL_fmt('Tried to reply more than once! Juan, FIX ME'));
    }
  }

  function sendErrorResponse(code) {
    var response = formatWebPageResponse(GnubbyMsgTypes.SIGN_WEB_REPLY, code);
    sendResponseOnce(response);
  }

  function sendSuccessResponse(challenge, info, browserData) {
    var responseData = {};
    for (var k in challenge) {
      responseData[k] = challenge[k];
    }
    responseData['browserData'] = B64_encode(UTIL_StringToBytes(browserData));
    responseData['signatureData'] = info;
    var response = formatWebPageResponse(GnubbyMsgTypes.SIGN_WEB_REPLY,
        GnubbyCodeTypes.OK, responseData);
    sendResponseOnce(response);
  }

  var origin = getOriginFromUrl(/** @type {string} */ (sender.url));
  if (!origin) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }
  // More closure type inference fail.
  var nonNullOrigin = /** @type {string} */ (origin);

  if (!isValidSignRequest(request)) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  var signData = request['signData'];
  // A valid sign data has at least one challenge, so get the first appId from
  // the first challenge.
  var firstAppId = signData[0]['appId'];
  var timeoutMillis = Signer.DEFAULT_TIMEOUT_MILLIS;
  if (request['timeout']) {
    // Request timeout is in seconds.
    timeoutMillis = request['timeout'] * 1000;
  }
  var timer = new CountdownTimer(timeoutMillis);
  var logMsgUrl = request['logMsgUrl'];

  // Queue sign requests from the same origin, to protect against simultaneous
  // sign-out on many tabs resulting in repeated sign-in requests.
  var queuedSignRequest = new QueuedSignRequest(signData, factory, timer,
      nonNullOrigin, sendErrorResponse, sendSuccessResponse,
      sender.tlsChannelId, logMsgUrl);
  var requestToken = signRequestQueue.queueRequest(firstAppId, nonNullOrigin,
      queuedSignRequest.begin.bind(queuedSignRequest), timer);
  queuedSignRequest.setToken(requestToken);
  return queuedSignRequest;
}

/**
 * Returns whether the request appears to be a valid sign request.
 * @param {Object} request the request.
 * @return {boolean} whether the request appears valid.
 */
function isValidSignRequest(request) {
  if (!request.hasOwnProperty('signData'))
    return false;
  var signData = request['signData'];
  // If a sign request contains an empty array of challenges, it could never
  // be fulfilled. Fail.
  if (!signData.length)
    return false;
  return isValidSignData(signData);
}

/**
 * Adapter class representing a queued sign request.
 * @param {!SignData} signData Signature data
 * @param {!SignHelperFactory} factory Factory for SignHelper instances
 * @param {Countdown} timer Timeout timer
 * @param {string} origin Signature origin
 * @param {function(number)} errorCb Error callback
 * @param {function(SignChallenge, string, string)} successCb Success callback
 * @param {string|undefined} opt_tlsChannelId TLS Channel Id
 * @param {string|undefined} opt_logMsgUrl Url to post log messages to
 * @constructor
 * @implements {Closeable}
 */
function QueuedSignRequest(signData, factory, timer, origin, errorCb, successCb,
    opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {!SignData} */
  this.signData_ = signData;
  /** @private {!SignHelperFactory} */
  this.factory_ = factory;
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(SignChallenge, string, string)} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;
  /** @private {boolean} */
  this.begun_ = false;
  /** @private {boolean} */
  this.closed_ = false;
}

/** Closes this sign request. */
QueuedSignRequest.prototype.close = function() {
  if (this.closed_) return;
  if (this.begun_ && this.signer_) {
    this.signer_.close();
  }
  if (this.token_) {
    this.token_.complete();
  }
  this.closed_ = true;
};

/**
 * @param {QueuedRequestToken} token Token for this sign request.
 */
QueuedSignRequest.prototype.setToken = function(token) {
  /** @private {QueuedRequestToken} */
  this.token_ = token;
};

/**
 * Called when this sign request may begin work.
 * @param {QueuedRequestToken} token Token for this sign request.
 */
QueuedSignRequest.prototype.begin = function(token) {
  this.begun_ = true;
  this.setToken(token);
  this.signer_ = new Signer(this.factory_, this.timer_, this.origin_,
      this.signerFailed_.bind(this), this.signerSucceeded_.bind(this),
      this.tlsChannelId_, this.logMsgUrl_);
  if (!this.signer_.setChallenges(this.signData_)) {
    token.complete();
    this.errorCb_(GnubbyCodeTypes.BAD_REQUEST);
  }
};

/**
 * Called when this request's signer fails.
 * @param {number} code The failure code reported by the signer.
 * @private
 */
QueuedSignRequest.prototype.signerFailed_ = function(code) {
  this.token_.complete();
  this.errorCb_(code);
};

/**
 * Called when this request's signer succeeds.
 * @param {SignChallenge} challenge The challenge that was signed.
 * @param {string} info The sign result.
 * @param {string} browserData Browser data JSON
 * @private
 */
QueuedSignRequest.prototype.signerSucceeded_ =
    function(challenge, info, browserData) {
  this.token_.complete();
  this.successCb_(challenge, info, browserData);
};

/**
 * Creates an object to track signing with a gnubby.
 * @param {!SignHelperFactory} helperFactory Factory to create a sign helper.
 * @param {Countdown} timer Timer for sign request.
 * @param {string} origin The origin making the request.
 * @param {function(number)} errorCb Called when the sign operation fails.
 * @param {function(SignChallenge, string, string)} successCb Called when the
 *     sign operation succeeds.
 * @param {string=} opt_tlsChannelId the TLS channel ID, if any, of the origin
 *     making the request.
 * @param {string=} opt_logMsgUrl The url to post log messages to.
 * @constructor
 */
function Signer(helperFactory, timer, origin, errorCb, successCb,
    opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(SignChallenge, string, string)} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {boolean} */
  this.challengesSet_ = false;
  /** @private {boolean} */
  this.done_ = false;

  /** @private {Object.<string, string>} */
  this.browserData_ = {};
  /** @private {Object.<string, SignChallenge>} */
  this.serverChallenges_ = {};
  // Allow http appIds for http origins. (Broken, but the caller deserves
  // what they get.)
  /** @private {boolean} */
  this.allowHttp_ = this.origin_ ? this.origin_.indexOf('http://') == 0 : false;

  // Protect against helper failure with a watchdog.
  this.createWatchdog_(timer);
  /** @private {SignHelper} */
  this.helper_ = helperFactory.createHelper();
}

/**
 * Creates a timer with an expiry greater than the expiration time of the given
 * timer.
 * @param {Countdown} timer Timeout timer
 * @private
 */
Signer.prototype.createWatchdog_ = function(timer) {
  var millis = timer.millisecondsUntilExpired();
  millis += CountdownTimer.TIMER_INTERVAL_MILLIS;
  /** @private {Countdown|undefined} */
  this.watchdogTimer_ = new CountdownTimer(millis, this.timeout_.bind(this));
};

/**
 * Default timeout value in case the caller never provides a valid timeout.
 */
Signer.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Sets the challenges to be signed.
 * @param {SignData} signData The challenges to set.
 * @return {boolean} Whether the challenges could be set.
 */
Signer.prototype.setChallenges = function(signData) {
  if (this.challengesSet_ || this.done_)
    return false;
  /** @private {SignData} */
  this.signData_ = signData;
  /** @private {boolean} */
  this.challengesSet_ = true;

  this.checkAppIds_();
  return true;
};

/**
 * Checks the app ids of incoming requests.
 * @private
 */
Signer.prototype.checkAppIds_ = function() {
  var appIds = getDistinctAppIds(this.signData_);
  if (!appIds || !appIds.length) {
    this.notifyError_(GnubbyCodeTypes.BAD_REQUEST);
    return;
  }
  /** @private {!AppIdChecker} */
  this.appIdChecker_ = new AppIdChecker(this.timer_.clone(), this.origin_,
      /** @type {!Array.<string>} */ (appIds), this.allowHttp_,
      this.logMsgUrl_);
  this.appIdChecker_.doCheck(this.appIdChecked_.bind(this));
};

/**
 * Called with the result of checking app ids.  When the app ids are valid,
 * adds the sign challenges to those being signed.
 * @param {boolean} result Whether the app ids are valid.
 * @private
 */
Signer.prototype.appIdChecked_ = function(result) {
  if (!result) {
    this.notifyError_(GnubbyCodeTypes.BAD_APP_ID);
    return;
  }
  if (!this.doSign_()) {
    this.notifyError_(GnubbyCodeTypes.BAD_REQUEST);
    return;
  }
};

/**
 * Begins signing this signer's challenges.
 * @return {boolean} Whether the challenge could be added.
 * @private
 */
Signer.prototype.doSign_ = function() {
  var encodedChallenges = [];
  for (var i = 0; i < this.signData_.length; i++) {
    var incomingChallenge = this.signData_[i];
    var serverChallenge = incomingChallenge['challenge'];
    var appId = incomingChallenge['appId'];
    var encodedKeyHandle = incomingChallenge['keyHandle'];
    var version = incomingChallenge['version'];

    var browserData =
        makeSignBrowserData(serverChallenge, this.origin_, this.tlsChannelId_);
    var encodedChallenge = makeChallenge(browserData, appId, encodedKeyHandle,
        version);

    var key = encodedKeyHandle + encodedChallenge['challengeHash'];
    this.browserData_[key] = browserData;
    this.serverChallenges_[key] = incomingChallenge;

    encodedChallenges.push(encodedChallenge);
  }
  var timeoutSeconds = this.timer_.millisecondsUntilExpired() / 1000.0;
  var request = makeSignHelperRequest(encodedChallenges, timeoutSeconds,
      this.logMsgUrl_);
  return this.helper_.doSign(request, this.helperComplete_.bind(this));
};

/**
 * Called when the timeout expires on this signer.
 * @private
 */
Signer.prototype.timeout_ = function() {
  this.watchdogTimer_ = undefined;
  // The web page gets grumpy if it doesn't get WAIT_TOUCH within a reasonable
  // time.
  this.notifyError_(GnubbyCodeTypes.WAIT_TOUCH);
};

/** Closes this signer. */
Signer.prototype.close = function() {
  if (this.appIdChecker_) {
    this.appIdChecker_.close();
  }
  if (this.helper_) {
    this.helper_.close();
  }
};

/**
 * Notifies the caller of error with the given error code.
 * @param {number} code Error code
 * @private
 */
Signer.prototype.notifyError_ = function(code) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.errorCb_(code);
};

/**
 * Notifies the caller of success.
 * @param {SignChallenge} challenge The challenge that was signed.
 * @param {string} info The sign result.
 * @param {string} browserData Browser data JSON
 * @private
 */
Signer.prototype.notifySuccess_ = function(challenge, info, browserData) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.successCb_(challenge, info, browserData);
};

/**
 * Maps a sign helper's error code namespace to the page's error code namespace.
 * @param {number} code Error code from DeviceStatusCodes namespace.
 * @return {number} A GnubbyCodeTypes error code.
 * @private
 */
Signer.mapError_ = function(code) {
  var reportedError;
  switch (code) {
    case DeviceStatusCodes.WRONG_DATA_STATUS:
      reportedError = GnubbyCodeTypes.NONE_PLUGGED_ENROLLED;
      break;

    case DeviceStatusCodes.TIMEOUT_STATUS:
    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
      reportedError = GnubbyCodeTypes.WAIT_TOUCH;
      break;

    default:
      reportedError = GnubbyCodeTypes.UNKNOWN_ERROR;
      break;
  }
  return reportedError;
};

/**
 * Called by the helper upon completion.
 * @param {SignHelperReply} reply The result of the sign request.
 * @param {string=} opt_source The source of the sign result.
 * @private
 */
Signer.prototype.helperComplete_ = function(reply, opt_source) {
  this.clearTimeout_();

  if (reply.code) {
    var reportedError = Signer.mapError_(reply.code);
    console.log(UTIL_fmt('helper reported ' + reply.code.toString(16) +
        ', returning ' + reportedError));
    this.notifyError_(reportedError);
  } else {
    if (this.logMsgUrl_ && opt_source) {
      var logMsg = 'signed&source=' + opt_source;
      logMessage(logMsg, this.logMsgUrl_);
    }

    var key =
        reply.responseData['keyHandle'] + reply.responseData['challengeHash'];
    var browserData = this.browserData_[key];
    // Notify with server-provided challenge, not the encoded one: the
    // server-provided challenge contains additional fields it relies on.
    var serverChallenge = this.serverChallenges_[key];
    this.notifySuccess_(serverChallenge, reply.responseData.signatureData,
        browserData);
  }
};

/**
 * Clears the timeout for this signer.
 * @private
 */
Signer.prototype.clearTimeout_ = function() {
  if (this.watchdogTimer_) {
    this.watchdogTimer_.clearTimeout();
    this.watchdogTimer_ = undefined;
  }
};
