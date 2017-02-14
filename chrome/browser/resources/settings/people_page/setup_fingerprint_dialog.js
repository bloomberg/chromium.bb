// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * The steps in the fingerprint setup flow.
 * @enum {number}
 */
var FingerprintSetupStep = {
  LOCATE_SCANNER: 1,      // The user needs to locate the scanner.
  MOVE_FINGER: 2,         // The user needs to move finger around the scanner.
  READY: 3                // The scanner has read the fingerprint successfully.
};

/**
 * The estimated amount of complete scans needed to enroll a fingerprint. Used
 * to help us estimate the progress of an enroll session.
 * @const {number}
 */
var SUCCESSFUL_SCANS_TO_COMPLETE = 5;

Polymer({
  is: 'settings-setup-fingerprint-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The problem message to display.
     * @private
     */
    problemMessage_: {
      type: String,
      value: '',
    },

    /**
     * The setup phase we are on.
     * @type {!FingerprintSetupStep}
     * @private
     */
    step_: {
      type: Number,
      value: FingerprintSetupStep.LOCATE_SCANNER
    },
  },

  /**
   * The number of scans that have been received during setup. This is used to
   * approximate the progress of the setup.
   * @type {number}
   * @private
   */
  receivedScanCount_: 0,

  /**
   * Opens the dialog.
   */
  open: function() {
    this.$.arc.drawBackgroundCircle();
    this.$.arc.drawShadow(10, 0, 0);
    this.$.dialog.showModal();
  },

  /**
   * Closes the dialog.
   */
  close: function() {
    this.reset_();
  },

  /**
   * Resets the dialog to its start state. Call this when the dialog gets
   * closed.
   * @private
   */
  reset_: function() {
    this.step_ = FingerprintSetupStep.LOCATE_SCANNER;
    this.receivedScanCount_ = 0;
    this.$.arc.clearCanvas();
  },

  /**
   * Checks if the the fingerprint can be submitted and added.
   * @private
   * @return {boolean}
   */
  enableContinue_: function(step) {
    return step == FingerprintSetupStep.READY;
  },

  /**
   * Closes the dialog.
   * @private
   */
  onCancelTap_: function() {
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * Closes the dialog.
   * @private
   */
  onContinueTap_: function() {
    this.fire('add-fingerprint');
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * Function to help test functionality without the fingerprint sensor.
   * TODO(sammiequon): Remove this when the fingerprint proxy is ready.
   * @param {Event} e The mouse click event.
   * @private
   */
  handleClick_: function(e) {
    this.onScanReceived_(settings.FingerprintResultType.SUCCESS,
        this.receivedScanCount_ == SUCCESSFUL_SCANS_TO_COMPLETE - 1);
  },

  /**
   * Function to help test functionality without the fingerprint sensor.
   * TODO(sammiequon): Remove this when the fingerprint proxy is ready.
   * @param {Event} e The mouse click event.
   * @private
   */
  handleDoubleClick_: function(e) {
    this.onScanReceived_(settings.FingerprintResultType.TOO_FAST,
        false /*isComplete*/);
  },

  /**
   * Advances steps, shows problems and animates the progress as needed based on
   * scan results.
   * @param {!settings.FingerprintResultType} scanResult The result from the
   *     scanner.
   * @param {bool} isComplete Whether the enroll session is complete.
   * @private
   */
  onScanReceived_: function(scanResult, isComplete) {
    switch (this.step_) {
      case FingerprintSetupStep.LOCATE_SCANNER:
      case FingerprintSetupStep.MOVE_FINGER:
        if (this.step_ == FingerprintSetupStep.LOCATE_SCANNER) {
          // Clear canvas because there will be shadows present at this step.
          this.$.arc.clearCanvas();
          this.$.arc.drawBackgroundCircle();

          this.step_ = FingerprintSetupStep.MOVE_FINGER;
          this.receivedScanCount_ = 0;
        }
        var slice = 2 * Math.PI / SUCCESSFUL_SCANS_TO_COMPLETE;
        if (isComplete) {
          this.problemMessage_ = '';
          this.step_ = FingerprintSetupStep.READY;
          this.$.arc.animate(this.receivedScanCount_ * slice, 2 * Math.PI);
        } else if (scanResult != settings.FingerprintResultType.SUCCESS) {
          this.setProblem_(scanResult);
        } else {
          this.problemMessage_ = '';
          this.$.arc.animate(this.receivedScanCount_ * slice,
              (this.receivedScanCount_ + 1) * slice);
          this.receivedScanCount_++;
        }
        break;
      case FingerprintSetupStep.READY:
        break;
      default:
        assertNotReached();
        break;
    }
  },

  /**
   * Sets the instructions based on which phase of the fingerprint setup we are
   * on.
   * @param {!FingerprintSetupStep} step The current step the fingerprint setup
   *     is on.
   * @private
   */
  getInstructionMessage_: function(step) {
    switch (step) {
      case FingerprintSetupStep.LOCATE_SCANNER:
        return this.i18n('configureFingerprintInstructionLocateScannerStep');
      case FingerprintSetupStep.MOVE_FINGER:
        return this.i18n('configureFingerprintInstructionMoveFingerStep');
      case FingerprintSetupStep.READY:
        return this.i18n('configureFingerprintInstructionReadyStep');
    }
    assertNotReached();
  },

  /**
   * Set the problem message based on the result from the fingerprint scanner.
   * @param {!settings.FingerprintResultType} scanResult The result the
   *     fingerprint scanner gives.
   * @private
   */
  setProblem_: function(scanResult) {
    switch (scanResult) {
      case settings.FingerprintResultType.SUCCESS:
        this.problemMessage_ = '';
        break;
      case settings.FingerprintResultType.PARTIAL_DATA:
        this.problemMessage_ = this.i18n('configureFingerprintPartialData');
        break;
      case settings.FingerprintResultType.INSUFFICIENT_DATA:
        this.problemMessage_ =
            this.i18n('configureFingerprintInsufficientData');
        break;
      case settings.FingerprintResultType.SENSOR_DIRTY:
        this.problemMessage_ = this.i18n('configureFingerprintSensorDirty');
        break;
      case settings.FingerprintResultType.TOO_SLOW:
        this.problemMessage_ = this.i18n('configureFingerprintTooSlow');
        break;
      case settings.FingerprintResultType.TOO_FAST:
        this.problemMessage_ = this.i18n('configureFingerprintTooFast');
        break;
      default:
        assertNotReached();
        break;
    }
  },
});
})();
