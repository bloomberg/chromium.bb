// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements an enroll helper using USB gnubbies.
 */
'use strict';

/**
 * @param {!GnubbyFactory} gnubbyFactory A factory for Gnubby instances.
 * @param {!CountdownFactory} timerFactory A factory to create timers.
 * @constructor
 * @implements {EnrollHelper}
 */
function UsbEnrollHelper(gnubbyFactory, timerFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
  /** @private {!CountdownFactory} */
  this.timerFactory_ = timerFactory;

  /** @private {Array.<usbGnubby>} */
  this.waitingForTouchGnubbies_ = [];

  /** @private {boolean} */
  this.closed_ = false;
  /** @private {boolean} */
  this.notified_ = false;
  /** @private {boolean} */
  this.signerComplete_ = false;
}

/**
 * Default timeout value in case the caller never provides a valid timeout.
 * @const
 */
UsbEnrollHelper.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Attempts to enroll using the provided data.
 * @param {EnrollHelperRequest} request The enroll request.
 * @param {function(EnrollHelperReply)} cb Called back with the result of the
 *     enroll request.
 */
UsbEnrollHelper.prototype.doEnroll = function(request, cb) {
  var timeoutMillis =
      request.timeout ?
      request.timeout * 1000 :
      UsbEnrollHelper.DEFAULT_TIMEOUT_MILLIS;
  /** @private {Countdown} */
  this.timer_ = this.timerFactory_.createTimer(timeoutMillis);
  this.enrollChallenges = request.enrollChallenges;
  /** @private {function(EnrollHelperReply)} */
  this.cb_ = cb;
  this.signer_ = new MultipleGnubbySigner(
      this.gnubbyFactory_,
      this.timerFactory_,
      true /* forEnroll */,
      this.signerCompleted_.bind(this),
      this.signerFoundGnubby_.bind(this),
      timeoutMillis,
      request.logMsgUrl);
  this.signer_.doSign(request.signData);
};

/** Closes this helper. */
UsbEnrollHelper.prototype.close = function() {
  this.closed_ = true;
  for (var i = 0; i < this.waitingForTouchGnubbies_.length; i++) {
    this.waitingForTouchGnubbies_[i].closeWhenIdle();
  }
  this.waitingForTouchGnubbies_ = [];
  if (this.signer_) {
    this.signer_.close();
    this.signer_ = null;
  }
};

/**
 * Called when a MultipleGnubbySigner completes its sign request.
 * @param {boolean} anyPending Whether any gnubbies are pending.
 * @private
 */
UsbEnrollHelper.prototype.signerCompleted_ = function(anyPending) {
  this.signerComplete_ = true;
  if (!this.anyGnubbiesFound_ || this.anyTimeout_ || anyPending ||
      this.timer_.expired()) {
    this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS);
  } else {
    // Do nothing: signerFoundGnubby will have been called with each succeeding
    // gnubby.
  }
};

/**
 * Called when a MultipleGnubbySigner finds a gnubby that can enroll.
 * @param {MultipleSignerResult} signResult Signature results
 * @param {boolean} moreExpected Whether the signer expects to report
 *     results from more gnubbies.
 * @private
 */
UsbEnrollHelper.prototype.signerFoundGnubby_ =
    function(signResult, moreExpected) {
  if (!signResult.code) {
    // If the signer reports a gnubby can sign, report this immediately to the
    // caller, as the gnubby is already enrolled.
    this.notifyError_(signResult.code);
  } else if (signResult.code == DeviceStatusCodes.WRONG_DATA_STATUS) {
    this.anyGnubbiesFound_ = true;
    var gnubby = signResult['gnubby'];
    this.waitingForTouchGnubbies_.push(gnubby);
    this.matchEnrollVersionToGnubby_(gnubby);
  }
};

/**
 * Attempts to match the gnubby's U2F version with an appropriate enroll
 * challenge.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.matchEnrollVersionToGnubby_ = function(gnubby) {
  if (!gnubby) {
    console.warn(UTIL_fmt('no gnubby, WTF?'));
  }
  gnubby.version(this.gnubbyVersioned_.bind(this, gnubby));
};

/**
 * Called with the result of a version command.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {number} rc result of version command.
 * @param {ArrayBuffer=} data version.
 * @private
 */
UsbEnrollHelper.prototype.gnubbyVersioned_ = function(gnubby, rc, data) {
  if (rc) {
    this.removeWrongVersionGnubby_(gnubby);
    return;
  }
  var version = UTIL_BytesToString(new Uint8Array(data || null));
  this.tryEnroll_(gnubby, version);
};

/**
 * Drops the gnubby from the list of eligible gnubbies.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.removeWaitingGnubby_ = function(gnubby) {
  gnubby.closeWhenIdle();
  var index = this.waitingForTouchGnubbies_.indexOf(gnubby);
  if (index >= 0) {
    this.waitingForTouchGnubbies_.splice(index, 1);
  }
};

/**
 * Drops the gnubby from the list of eligible gnubbies, as it has the wrong
 * version.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.removeWrongVersionGnubby_ = function(gnubby) {
  this.removeWaitingGnubby_(gnubby);
  if (!this.waitingForTouchGnubbies_.length) {
    // Whoops, this was the last gnubby.
    this.anyGnubbiesFound_ = false;
    if (this.timer_.expired()) {
      this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS);
    } else {
      // TODO: add method to MultipleGnubbySigner to signal gnubby
      // gone?
    }
  }
};

/**
 * Attempts enrolling a particular gnubby with a challenge of the appropriate
 * version.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {string} version Protocol version
 * @private
 */
UsbEnrollHelper.prototype.tryEnroll_ = function(gnubby, version) {
  var challenge = this.getChallengeOfVersion_(version);
  if (!challenge) {
    this.removeWrongVersionGnubby_(gnubby);
    return;
  }
  var challengeChallenge = B64_decode(challenge['challenge']);
  var appIdHash = B64_decode(challenge['appIdHash']);
  gnubby.enroll(challengeChallenge, appIdHash,
      this.enrollCallback_.bind(this, gnubby, version));
};

/**
 * Finds the (first) challenge of the given version in this helper's challenges.
 * @param {string} version Protocol version
 * @return {Object} challenge, if found, or null if not.
 * @private
 */
UsbEnrollHelper.prototype.getChallengeOfVersion_ = function(version) {
  for (var i = 0; i < this.enrollChallenges.length; i++) {
    if (this.enrollChallenges[i]['version'] == version) {
      return this.enrollChallenges[i];
    }
  }
  return null;
};

/**
 * Called with the result of an enroll request to a gnubby.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {string} version Protocol version
 * @param {number} code Status code
 * @param {ArrayBuffer=} infoArray Returned data
 * @private
 */
UsbEnrollHelper.prototype.enrollCallback_ =
    function(gnubby, version, code, infoArray) {
  if (this.notified_) {
    // Enroll completed after previous success or failure. Disregard.
    return;
  }
  switch (code) {
    case -llGnubby.GONE:
        // Close this gnubby.
        this.removeWaitingGnubby_(gnubby);
        if (!this.waitingForTouchGnubbies_.length) {
          // Last enroll attempt is complete and last gnubby is gone.
          this.anyGnubbiesFound_ = false;
          if (this.timer_.expired()) {
            this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS);
          } else {
            // TODO: add method to MultipleGnubbySigner to signal
            // gnubby gone.
          }
        }
      break;

    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
    case DeviceStatusCodes.BUSY_STATUS:
    case DeviceStatusCodes.TIMEOUT_STATUS:
      if (this.timer_.expired()) {
        // Record that at least one gnubby timed out, to return a timeout status
        // from the complete callback if no other eligible gnubbies are found.
        /** @private {boolean} */
        this.anyTimeout_ = true;
        // Close this gnubby.
        this.removeWaitingGnubby_(gnubby);
        if (!this.waitingForTouchGnubbies_.length) {
          // Last enroll attempt is complete: return this error.
          console.log(UTIL_fmt('timeout (' + code.toString(16) +
              ') enrolling'));
          this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS);
        }
      } else {
        this.timerFactory_.createTimer(
            UsbEnrollHelper.ENUMERATE_DELAY_INTERVAL_MILLIS,
            this.tryEnroll_.bind(this, gnubby, version));
      }
      break;

    case DeviceStatusCodes.OK_STATUS:
      var info = B64_encode(new Uint8Array(infoArray || []));
      this.notifySuccess_(version, info);
      break;

    default:
      console.log(UTIL_fmt('Failed to enroll gnubby: ' + code));
      this.notifyError_(code);
      break;
  }
};

/**
 * How long to delay between repeated enroll attempts, in milliseconds.
 * @const
 */
UsbEnrollHelper.ENUMERATE_DELAY_INTERVAL_MILLIS = 200;

/**
 * Notifies the callback with an error code.
 * @param {number} code The error code to report.
 * @private
 */
UsbEnrollHelper.prototype.notifyError_ = function(code) {
  if (this.notified_ || this.closed_)
    return;
  this.notified_ = true;
  this.close();
  var reply = {
    'type': 'enroll_helper_reply',
    'code': code
  };
  this.cb_(reply);
};

/**
 * @param {string} version Protocol version
 * @param {string} info B64 encoded success data
 * @private
 */
UsbEnrollHelper.prototype.notifySuccess_ = function(version, info) {
  if (this.notified_ || this.closed_)
    return;
  this.notified_ = true;
  this.close();
  var reply = {
    'type': 'enroll_helper_reply',
    'code': DeviceStatusCodes.OK_STATUS,
    'version': version,
    'enrollData': info
  };
  this.cb_(reply);
};

/**
 * @param {!GnubbyFactory} gnubbyFactory Factory to create gnubbies.
 * @param {!CountdownFactory} timerFactory Used to create timers to reenumerate
 *     gnubbies.
 * @constructor
 * @implements {EnrollHelperFactory}
 */
function UsbEnrollHelperFactory(gnubbyFactory, timerFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
  /** @private {!CountdownFactory} */
  this.timerFactory_ = timerFactory;
}

/**
 * @return {UsbEnrollHelper} the newly created helper.
 */
UsbEnrollHelperFactory.prototype.createHelper = function() {
  return new UsbEnrollHelper(this.gnubbyFactory_, this.timerFactory_);
};
