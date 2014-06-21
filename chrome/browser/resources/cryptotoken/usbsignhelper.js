// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a sign helper using USB gnubbies.
 */
'use strict';

var CORRUPT_sign = false;

/**
 * @param {!GnubbyFactory} gnubbyFactory Factory for gnubby instances
 * @param {!CountdownFactory} timerFactory A factory to create timers.
 * @constructor
 * @implements {SignHelper}
 */
function UsbSignHelper(gnubbyFactory, timerFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
  /** @private {!CountdownFactory} */
  this.timerFactory_ = timerFactory;

  /** @private {Array.<usbGnubby>} */
  this.waitingForTouchGnubbies_ = [];

  /** @private {boolean} */
  this.notified_ = false;
  /** @private {boolean} */
  this.signerComplete_ = false;
  /** @private {boolean} */
  this.anyGnubbiesFound_ = false;
}

/**
 * Default timeout value in case the caller never provides a valid timeout.
 * @const
 */
UsbSignHelper.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Attempts to sign the provided challenges.
 * @param {SignHelperRequest} request The sign request.
 * @param {function(SignHelperReply, string=)} cb Called with the result of the
 *     sign request and an optional source for the sign result.
 * @return {boolean} whether this set of challenges was accepted.
 */
UsbSignHelper.prototype.doSign = function(request, cb) {
  if (this.cb_) {
    // Can only handle one request.
    return false;
  }
  /** @private {function(SignHelperReply, string=)} */
  this.cb_ = cb;
  if (!request.signData || !request.signData.length) {
    // Fail a sign request with an empty set of challenges, and pretend to have
    // alerted the caller in case the enumerate is still pending.
    this.notified_ = true;
    return false;
  }
  var timeoutMillis =
      request.timeout ?
      request.timeout * 1000 :
      UsbSignHelper.DEFAULT_TIMEOUT_MILLIS;
  /** @private {MultipleGnubbySigner} */
  this.signer_ = new MultipleGnubbySigner(
      this.gnubbyFactory_,
      this.timerFactory_,
      false /* forEnroll */,
      this.signerCompleted_.bind(this),
      this.signerFoundGnubby_.bind(this),
      timeoutMillis,
      request.logMsgUrl);
  return this.signer_.doSign(request.signData);
};


/**
 * Called when a MultipleGnubbySigner completes.
 * @param {boolean} anyPending Whether any gnubbies are pending.
 * @private
 */
UsbSignHelper.prototype.signerCompleted_ = function(anyPending) {
  this.signerComplete_ = true;
  if (!this.anyGnubbiesFound_ || anyPending) {
    this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS);
  } else if (this.signerError_ !== undefined) {
    this.notifyError_(this.signerError_);
  } else {
    // Do nothing: signerFoundGnubby_ will have returned results from other
    // gnubbies.
  }
};

/**
 * Called when a MultipleGnubbySigner finds a gnubby that has completed signing
 * its challenges.
 * @param {MultipleSignerResult} signResult Signer result object
 * @param {boolean} moreExpected Whether the signer expects to produce more
 *     results.
 * @private
 */
UsbSignHelper.prototype.signerFoundGnubby_ =
    function(signResult, moreExpected) {
  this.anyGnubbiesFound_ = true;
  if (!signResult.code) {
    var gnubby = signResult['gnubby'];
    var challenge = signResult['challenge'];
    var info = new Uint8Array(signResult['info']);
    this.notifySuccess_(gnubby, challenge, info);
  } else if (!moreExpected) {
    // If the signer doesn't expect more results, return the error directly to
    // the caller.
    this.notifyError_(signResult.code);
  } else {
    // Record the last error, to report from the complete callback if no other
    // eligible gnubbies are found.
    /** @private {number} */
    this.signerError_ = signResult.code;
  }
};

/**
 * Reports the result of a successful sign operation.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {SignHelperChallenge} challenge Challenge signed
 * @param {Uint8Array} info Result data
 * @private
 */
UsbSignHelper.prototype.notifySuccess_ = function(gnubby, challenge, info) {
  if (this.notified_)
    return;
  this.notified_ = true;

  gnubby.closeWhenIdle();
  this.close();

  if (CORRUPT_sign) {
    CORRUPT_sign = false;
    info[info.length - 1] = info[info.length - 1] ^ 0xff;
  }
  var responseData = {
    'appIdHash': B64_encode(challenge['appIdHash']),
    'challengeHash': B64_encode(challenge['challengeHash']),
    'keyHandle': B64_encode(challenge['keyHandle']),
    'signatureData': B64_encode(info)
  };
  var reply = {
    'type': 'sign_helper_reply',
    'code': DeviceStatusCodes.OK_STATUS,
    'responseData': responseData
  };
  this.cb_(reply, 'USB');
};

/**
 * Reports error to the caller.
 * @param {number} code error to report
 * @private
 */
UsbSignHelper.prototype.notifyError_ = function(code) {
  if (this.notified_)
    return;
  this.notified_ = true;
  this.close();
  var reply = {
    'type': 'sign_helper_reply',
    'code': code
  };
  this.cb_(reply);
};

/**
 * Closes the MultipleGnubbySigner, if any.
 */
UsbSignHelper.prototype.close = function() {
  if (this.signer_) {
    this.signer_.close();
    this.signer_ = null;
  }
  for (var i = 0; i < this.waitingForTouchGnubbies_.length; i++) {
    this.waitingForTouchGnubbies_[i].closeWhenIdle();
  }
  this.waitingForTouchGnubbies_ = [];
};

/**
 * @param {!GnubbyFactory} gnubbyFactory Factory to create gnubbies.
 * @param {!CountdownFactory} timerFactory A factory to create timers.
 * @constructor
 * @implements {SignHelperFactory}
 */
function UsbSignHelperFactory(gnubbyFactory, timerFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
  /** @private {!CountdownFactory} */
  this.timerFactory_ = timerFactory;
}

/**
 * @return {UsbSignHelper} the newly created helper.
 */
UsbSignHelperFactory.prototype.createHelper = function() {
  return new UsbSignHelper(this.gnubbyFactory_, this.timerFactory_);
};
