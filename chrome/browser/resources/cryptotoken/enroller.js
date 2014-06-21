// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles web page requests for gnubby enrollment.
 */

'use strict';

/**
 * Handles an enroll request.
 * @param {!EnrollHelperFactory} factory Factory to create an enroll helper.
 * @param {MessageSender} sender The sender of the message.
 * @param {Object} request The web page's enroll request.
 * @param {Function} sendResponse Called back with the result of the enroll.
 * @return {Closeable} A handler object to be closed when the browser channel
 *     closes.
 */
function handleEnrollRequest(factory, sender, request, sendResponse) {
  var sentResponse = false;
  function sendResponseOnce(r) {
    if (enroller) {
      enroller.close();
      enroller = null;
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
    console.log(UTIL_fmt('code=' + code));
    var response = formatWebPageResponse(GnubbyMsgTypes.ENROLL_WEB_REPLY, code);
    if (request['requestId']) {
      response['requestId'] = request['requestId'];
    }
    sendResponseOnce(response);
  }

  var origin = getOriginFromUrl(/** @type {string} */ (sender.url));
  if (!origin) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  if (!isValidEnrollRequest(request)) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  var signData = request['signData'];
  var enrollChallenges = request['enrollChallenges'];
  var logMsgUrl = request['logMsgUrl'];
  var timeoutMillis = Enroller.DEFAULT_TIMEOUT_MILLIS;
  if (request['timeout']) {
    // Request timeout is in seconds.
    timeoutMillis = request['timeout'] * 1000;
  }

  function findChallengeOfVersion(enrollChallenges, version) {
    for (var i = 0; i < enrollChallenges.length; i++) {
      if (enrollChallenges[i]['version'] == version) {
        return enrollChallenges[i];
      }
    }
    return null;
  }

  function sendSuccessResponse(u2fVersion, info, browserData) {
    var enrollChallenge = findChallengeOfVersion(enrollChallenges, u2fVersion);
    if (!enrollChallenge) {
      sendErrorResponse(GnubbyCodeTypes.UNKNOWN_ERROR);
      return;
    }
    var enrollUpdateData = {};
    enrollUpdateData['enrollData'] = info;
    // Echo the used challenge back in the reply.
    for (var k in enrollChallenge) {
      enrollUpdateData[k] = enrollChallenge[k];
    }
    if (u2fVersion == 'U2F_V2') {
      // For U2F_V2, the challenge sent to the gnubby is modified to be the
      // hash of the browser data. Include the browser data.
      enrollUpdateData['browserData'] = browserData;
    }
    var response = formatWebPageResponse(
        GnubbyMsgTypes.ENROLL_WEB_REPLY, GnubbyCodeTypes.OK, enrollUpdateData);
    sendResponseOnce(response);
  }

  var timer = new CountdownTimer(timeoutMillis);
  var enroller = new Enroller(factory, timer, origin, sendErrorResponse,
      sendSuccessResponse, sender.tlsChannelId, logMsgUrl);
  enroller.doEnroll(enrollChallenges, signData);
  return /** @type {Closeable} */ (enroller);
}

/**
 * Returns whether the request appears to be a valid enroll request.
 * @param {Object} request the request.
 * @return {boolean} whether the request appears valid.
 */
function isValidEnrollRequest(request) {
  if (!request.hasOwnProperty('enrollChallenges'))
    return false;
  var enrollChallenges = request['enrollChallenges'];
  if (!enrollChallenges.length)
    return false;
  var seenVersions = {};
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge['version'];
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }
    if (version != 'U2F_V1' && version != 'U2F_V2') {
      return false;
    }
    if (seenVersions[version]) {
      // Each version can appear at most once.
      return false;
    }
    seenVersions[version] = version;
    if (!enrollChallenge['appId']) {
      return false;
    }
    if (!enrollChallenge['challenge']) {
      // The challenge is required.
      return false;
    }
  }
  var signData = request['signData'];
  // An empty signData is ok, in the case the user is not already enrolled.
  if (signData && !isValidSignData(signData))
    return false;
  return true;
}

/**
 * Creates a new object to track enrolling with a gnubby.
 * @param {!EnrollHelperFactory} helperFactory factory to create an enroll
 *     helper.
 * @param {!Countdown} timer Timer for enroll request.
 * @param {string} origin The origin making the request.
 * @param {function(number)} errorCb Called upon enroll failure with an error
 *     code.
 * @param {function(string, string, (string|undefined))} successCb Called upon
 *     enroll success with the version of the succeeding gnubby, the enroll
 *     data, and optionally the browser data associated with the enrollment.
 * @param {string=} opt_tlsChannelId the TLS channel ID, if any, of the origin
 *     making the request.
 * @param {string=} opt_logMsgUrl The url to post log messages to.
 * @constructor
 */
function Enroller(helperFactory, timer, origin, errorCb, successCb,
    opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(string, string, (string|undefined))} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {boolean} */
  this.done_ = false;
  /** @private {number|undefined} */
  this.lastProgressUpdate_ = undefined;

  /** @private {Object.<string, string>} */
  this.browserData_ = {};
  /** @private {Array.<EnrollHelperChallenge>} */
  this.encodedEnrollChallenges_ = [];
  /** @private {Array.<SignHelperChallenge>} */
  this.encodedSignChallenges_ = [];
  // Allow http appIds for http origins. (Broken, but the caller deserves
  // what they get.)
  /** @private {boolean} */
  this.allowHttp_ = this.origin_ ? this.origin_.indexOf('http://') == 0 : false;

  /** @private {EnrollHelper} */
  this.helper_ = helperFactory.createHelper();
}

/**
 * Default timeout value in case the caller never provides a valid timeout.
 */
Enroller.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Performs an enroll request with the given enroll and sign challenges.
 * @param {Array.<Object>} enrollChallenges A set of enroll challenges
 * @param {Array.<Object>} signChallenges A set of sign challenges for existing
 *     enrollments for this user and appId
 */
Enroller.prototype.doEnroll = function(enrollChallenges, signChallenges) {
  var encodedEnrollChallenges = this.encodeEnrollChallenges_(enrollChallenges);
  var encodedSignChallenges = Enroller.encodeSignChallenges_(signChallenges);
  var request = {
    type: 'enroll_helper_request',
    enrollChallenges: encodedEnrollChallenges,
    signData: encodedSignChallenges,
    logMsgUrl: this.logMsgUrl_
  };
  if (!this.timer_.expired()) {
    request.timeout = this.timer_.millisecondsUntilExpired() / 1000.0;
  }

  // Begin fetching/checking the app ids.
  var enrollAppIds = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    enrollAppIds.push(enrollChallenges[i]['appId']);
  }
  var self = this;
  this.checkAppIds_(enrollAppIds, signChallenges, function(result) {
    if (result) {
      self.helper_.doEnroll(request, self.helperComplete_.bind(self));
    } else {
      self.notifyError_(GnubbyCodeTypes.BAD_APP_ID);
    }
  });
};

/**
 * Encodes the enroll challenge as an enroll helper challenge.
 * @param {Object} enrollChallenge The enroll challenge to encode.
 * @return {EnrollHelperChallenge} The encoded challenge.
 * @private
 */
Enroller.encodeEnrollChallenge_ = function(enrollChallenge) {
  var encodedChallenge = {};
  var version;
  if (enrollChallenge['version']) {
    version = enrollChallenge['version'];
  } else {
    // Version is implicitly V1 if not specified.
    version = 'U2F_V1';
  }
  encodedChallenge['version'] = version;
  encodedChallenge['challenge'] = enrollChallenge['challenge'];
  encodedChallenge['appIdHash'] =
      B64_encode(sha256HashOfString(enrollChallenge['appId']));
  return /** @type {EnrollHelperChallenge} */ (encodedChallenge);
};

/**
 * Encodes the given enroll challenges using this enroller's state.
 * @param {Array.<Object>} enrollChallenges The enroll challenges.
 * @return {!Array.<EnrollHelperChallenge>} The encoded enroll challenges.
 * @private
 */
Enroller.prototype.encodeEnrollChallenges_ = function(enrollChallenges) {
  var challenges = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge.version;
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }

    if (version == 'U2F_V2') {
      var modifiedChallenge = {};
      for (var k in enrollChallenge) {
        modifiedChallenge[k] = enrollChallenge[k];
      }
      // V2 enroll responses contain signatures over a browser data object,
      // which we're constructing here. The browser data object contains, among
      // other things, the server challenge.
      var serverChallenge = enrollChallenge['challenge'];
      var browserData = makeEnrollBrowserData(
          serverChallenge, this.origin_, this.tlsChannelId_);
      // Replace the challenge with the hash of the browser data.
      modifiedChallenge['challenge'] =
          B64_encode(sha256HashOfString(browserData));
      this.browserData_[version] =
          B64_encode(UTIL_StringToBytes(browserData));
      challenges.push(Enroller.encodeEnrollChallenge_(modifiedChallenge));
    } else {
      challenges.push(Enroller.encodeEnrollChallenge_(enrollChallenge));
    }
  }
  return challenges;
};

/**
 * Encodes the sign data as an array of sign helper challenges.
 * @param {Array=} signData The sign challenges to encode.
 * @return {!Array.<SignHelperChallenge>} The sign challenges, encoded.
 * @private
 */
Enroller.encodeSignChallenges_ = function(signData) {
  var encodedSignChallenges = [];
  if (signData) {
    for (var i = 0; i < signData.length; i++) {
      var incomingChallenge = signData[i];
      var serverChallenge = incomingChallenge['challenge'];
      var appId = incomingChallenge['appId'];
      var encodedKeyHandle = incomingChallenge['keyHandle'];

      var challenge = makeChallenge(serverChallenge, appId, encodedKeyHandle,
          incomingChallenge['version']);

      encodedSignChallenges.push(challenge);
    }
  }
  return encodedSignChallenges;
};

/**
 * Checks the app ids associated with this enroll request, and calls a callback
 * with the result of the check.
 * @param {!Array.<string>} enrollAppIds The app ids in the enroll challenge
 *     portion of the enroll request.
 * @param {SignData} signData The sign data associated with the request.
 * @param {function(boolean)} cb Called with the result of the check.
 * @private
 */
Enroller.prototype.checkAppIds_ = function(enrollAppIds, signData, cb) {
  var distinctAppIds =
      UTIL_unionArrays(enrollAppIds, getDistinctAppIds(signData));
  /** @private {!AppIdChecker} */
  this.appIdChecker_ = new AppIdChecker(this.timer_.clone(), this.origin_,
      distinctAppIds, this.allowHttp_, this.logMsgUrl_);
  this.appIdChecker_.doCheck(cb);
};

/** Closes this enroller. */
Enroller.prototype.close = function() {
  if (this.appIdChecker_) {
    this.appIdChecker_.close();
  }
  if (this.helper_) {
    this.helper_.close();
  }
};

/**
 * Notifies the caller with the error code.
 * @param {number} code Error code
 * @private
 */
Enroller.prototype.notifyError_ = function(code) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.errorCb_(code);
};

/**
 * Notifies the caller of success with the provided response data.
 * @param {string} u2fVersion Protocol version
 * @param {string} info Response data
 * @param {string|undefined} opt_browserData Browser data used
 * @private
 */
Enroller.prototype.notifySuccess_ =
    function(u2fVersion, info, opt_browserData) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.successCb_(u2fVersion, info, opt_browserData);
};

/**
 * Notifies the caller of progress with the error code.
 * @param {number} code Status code
 * @private
 */
Enroller.prototype.notifyProgress_ = function(code) {
  if (this.done_)
    return;
  if (code != this.lastProgressUpdate_) {
    this.lastProgressUpdate_ = code;
    // If there is no progress callback, treat it like an error and clean up.
    if (this.progressCb_) {
      this.progressCb_(code);
    } else {
      this.notifyError_(code);
    }
  }
};

/**
 * Maps an enroll helper's error code namespace to the page's error code
 * namespace.
 * @param {number} code Error code from DeviceStatusCodes namespace.
 * @return {number} A GnubbyCodeTypes error code.
 * @private
 */
Enroller.mapError_ = function(code) {
  var reportedError = GnubbyCodeTypes.UNKNOWN_ERROR;
  switch (code) {
    case DeviceStatusCodes.OK_STATUS:
      reportedError = GnubbyCodeTypes.ALREADY_ENROLLED;
      break;

    case DeviceStatusCodes.TIMEOUT_STATUS:
      reportedError = GnubbyCodeTypes.WAIT_TOUCH;
      break;
  }
  return reportedError;
};

/**
 * Called by the helper upon completion.
 * @param {EnrollHelperReply} reply The result of the enroll request.
 * @private
 */
Enroller.prototype.helperComplete_ = function(reply) {
  if (reply.code) {
    var reportedError = Enroller.mapError_(reply.code);
    console.log(UTIL_fmt('helper reported ' + reply.code.toString(16) +
        ', returning ' + reportedError));
    this.notifyError_(reportedError);
  } else {
    console.log(UTIL_fmt('Gnubby enrollment succeeded!!!!!'));
    var browserData;

    if (reply.version == 'U2F_V2') {
      // For U2F_V2, the challenge sent to the gnubby is modified to be the hash
      // of the browser data. Include the browser data.
      browserData = this.browserData_[reply.version];
    }

    this.notifySuccess_(/** @type {string} */ (reply.version),
                        /** @type {string} */ (reply.enrollData),
                        browserData);
  }
};
