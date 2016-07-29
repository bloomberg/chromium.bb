// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-quick-unlock-setup-pin' is the settings page for choosing a PIN.
 *
 * Example:
 *
 * <settings-quick-unlock-setup-pin
 *   set-modes="[[quickUnlockSetModes]]"
 *   current-route="{{currentRoute}}">
 * </settings-quick-unlock-setup-pin>
 */

(function() {
'use strict';

/**
 * Metainformation about a problem message to pass to showProblem. |class| is
 * the css class to show the problem message with. |messageId| is the i18n
 * string id to use.
 * @const
 */
var ProblemInfo = {
  TOO_SHORT: {
    messageId: 'quickUnlockConfigurePinChoosePinTooShort',
    class: 'error'
  },
  WEAK: {
    messageId: 'quickUnlockConfigurePinChoosePinWeakPinWarning',
    class: 'warning'
  },
  MISMATCHED: {
    messageId: 'quickUnlockConfigurePinMismatchedPins',
    class: 'error'
  }
};

/**
 * A list of the top-10 most commmonly used PINs. This list is taken from
 * www.datagenetics.com/blog/september32012/.
 * @const
 */
var WEAK_PINS = [
  '1234', '1111', '0000', '1212', '7777', '1004', '2000', '4444', '2222',
  '6969'
];

Polymer({
  is: 'settings-quick-unlock-setup-pin',

  behaviors: [I18nBehavior, QuickUnlockPasswordDetectBehavior],

  properties: {
    /** @type {!settings.Route} */
    currentRoute: {
      type: Object,
      observer: 'onRouteChanged_',
    },

    /**
     * The current PIN keyboard value.
     * @private
     */
    pinKeyboardValue_: String,

    /**
     * Stores the initial PIN value so it can be confirmed.
     * @private
     */
    initialPin_: String,

    /**
     * The actual problem message to display.
     * @private
     */
    problemMessage_: String,

    /**
     * The type of problem class to show (warning or error).
     */
    problemClass_: String,

    /**
     * Should the step-specific submit button be displayed?
     * @private
     */
    enableSubmit_: Boolean,

    /**
     * The current step/subpage we are on.
     * @private
     */
    isConfirmStep_: {
      type: Boolean,
      value: false
    },
  },

  observers: ['onSetModesChanged_(setModes)'],

  /** @override */
  attached: function() {
    this.resetState_();

    if (this.currentRoute == settings.Route.QUICK_UNLOCK_SETUP_PIN)
      this.askForPasswordIfUnset();
  },

  /**
   * @param {!settings.Route} currentRoute
   * @private
   */
  onRouteChanged_: function(currentRoute) {
    if (this.currentRoute == settings.Route.QUICK_UNLOCK_SETUP_PIN) {
      this.askForPasswordIfUnset();
    } else {
      // If the user hits the back button, they can leave the element
      // half-completed; therefore, reset state if the element is not active.
      this.resetState_();
    }
  },

  /** @private */
  onSetModesChanged_: function() {
    if (this.currentRoute == settings.Route.QUICK_UNLOCK_SETUP_PIN)
      this.askForPasswordIfUnset();
  },

  /**
   * Resets the element to the initial state.
   * @private
   */
  resetState_: function() {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.enableSubmit_ = false;
    this.isConfirmStep_ = false;
    this.onPinChange_();
  },

  /**
   * Returns true if the given PIN is likely easy to guess.
   * @private
   * @param {string} pin
   * @return {boolean}
   */
  isPinWeak_: function(pin) {
    // Warn if it's a top-10 pin.
    if (WEAK_PINS.includes(pin))
      return true;

    // Warn if the PIN is consecutive digits.
    var delta = 0;
    for (var i = 1; i < pin.length; ++i) {
      var prev = Number(pin[i - 1]);
      var num = Number(pin[i]);
      if (Number.isNaN(prev) || Number.isNaN(num))
        return false;
      delta = Math.max(delta, Math.abs(num - prev));
    }

    return delta <= 1;
  },

  /**
   * Returns true if the given PIN matches PIN requirements, such as minimum
   * length.
   * @private
   * @param {string|undefined} pin
   * @return {boolean}
   */
  isPinLongEnough_: function(pin) {
    return !!pin && pin.length >= 4;
  },

  /**
   * Returns true if the PIN is ready to be changed to a new value.
   * @private
   * @return {boolean}
   */
  canSubmit_: function() {
    return this.isPinLongEnough_(this.pinKeyboardValue_) &&
           this.initialPin_ == this.pinKeyboardValue_;
  },

  /**
   * Notify the user about a problem.
   * @private
   * @param {!{messageId: string, class: string}} problemInfo
   */
  showProblem_: function(problemInfo) {
    this.problemMessage_ = this.i18n(problemInfo.messageId);
    this.problemClass_ = problemInfo.class;
    this.updateStyles();
  },

  /** @private */
  hideProblem_: function() {
    this.problemMessage_ = '';
    this.problemClass_ = '';
  },

  /** @private */
  onPinChange_: function() {
    if (!this.isConfirmStep_) {
      var isPinLongEnough = this.isPinLongEnough_(this.pinKeyboardValue_);
      var isWeak = isPinLongEnough && this.isPinWeak_(this.pinKeyboardValue_);

      if (!isPinLongEnough && this.pinKeyboardValue_)
        this.showProblem_(ProblemInfo.TOO_SHORT);
      else if (isWeak)
        this.showProblem_(ProblemInfo.WEAK);
      else
        this.hideProblem_();

      this.enableSubmit_ = isPinLongEnough;

    } else {
      var canSubmit = this.canSubmit_();

      if (!canSubmit && this.pinKeyboardValue_)
        this.showProblem_(ProblemInfo.MISMATCHED);
      else
        this.hideProblem_();

      this.enableSubmit_ = canSubmit;
    }
  },

  /** @private */
  onPinSubmit_: function() {
    if (!this.isConfirmStep_) {
      if (this.isPinLongEnough_(this.pinKeyboardValue_)) {
        this.initialPin_ = this.pinKeyboardValue_;
        this.pinKeyboardValue_ = '';
        this.isConfirmStep_ = true;
        this.onPinChange_();
      }
    } else {
      // onPinSubmit_ gets called if the user hits enter on the PIN keyboard.
      // The PIN is not guaranteed to be valid in that case.
      if (!this.canSubmit_())
        return;

      function onSetModesCompleted(didSet) {
        if (!didSet) {
          console.error('Failed to update pin');
          return;
        }

        this.resetState_();
        settings.navigateTo(settings.Route.QUICK_UNLOCK_CHOOSE_METHOD);
      }

      this.setModes.call(
        null,
        [chrome.quickUnlockPrivate.QuickUnlockMode.PIN],
        [this.pinKeyboardValue_],
        onSetModesCompleted.bind(this));
    }
  },

  /**
   * @private
   * @param {string} problemMessage
   * @return {boolean}
   */
  hasProblem_: function(problemMessage) {
    return !!problemMessage;
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getTitleMessage_: function(isConfirmStep) {
    if (!isConfirmStep)
      return this.i18n('quickUnlockConfigurePinChoosePinTitle');
    return this.i18n('quickUnlockConfigurePinConfirmPinTitle');
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getContinueMessage_: function(isConfirmStep) {
    if (!isConfirmStep)
      return this.i18n('quickUnlockConfigurePinContinueButton');
    return this.i18n('save');
  },
});

})();
