// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pin-keyboard' is a keyboard that can be used to enter PINs or more generally
 * numeric values.
 *
 * Properties:
 *    value: The value of the PIN keyboard. Writing to this property will adjust
 *           the PIN keyboard's value.
 *
 * Events:
 *    pin-change: Fired when the PIN value has changed. The pin is available at
 *                event.detail.pin.
 *    submit: Fired when the PIN is submitted. The pin is available at
 *            event.detail.pin.
 *
 * Example:
 *    <pin-keyboard on-pin-change="onPinChange" on-submit="onPinSubmit"
 *                  value="{{pinValue}}">
 *    </pin-keyboard>
 */

(function() {

/**
 * Once auto backspace starts, the time between individual backspaces.
 * @type {number}
 * @const
 */
var REPEAT_BACKSPACE_DELAY_MS = 150;

/**
 * How long the backspace button must be held down before auto backspace
 * starts.
 * @type {number}
 * @const
 */
var INITIAL_BACKSPACE_DELAY_MS = 500;

Polymer({
  is: 'pin-keyboard',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Whether or not the keyboard's input element should be numerical
     * or password.
     */
    enablePassword: {
      type: Boolean,
      value: false
    },

    /**
     * Whether or not the keyboard's input should be shown.
     */
    hideInput: {
      type: Boolean,
      value: false
    },

    /** The value stored in the keyboard's input element. */
    value: {
      type: String,
      notify: true,
      value: '',
      observer: 'onPinValueChange_'
    },

    /**
     * The intervalID used for the backspace button set/clear interval.
     * @private
     */
    repeatBackspaceIntervalId_: {
      type: Number,
      value: 0
    },

    /**
     * The timeoutID used for the auto backspace.
     * @private
     */
    startAutoBackspaceId_: {
      type: Number,
      value: 0
    }
  },

  /**
   * @override
   */
  attached: function() {
    // Remove the space/enter key binds from the polymer
    // iron-a11y-keys-behavior.
    var digitButtons = Polymer.dom(this.root).querySelectorAll('.digit-button');
    for (var i = 0; i < digitButtons.length; ++i)
      digitButtons[i].keyEventTarget = null;
  },

  /**
   * Gets the container holding the password field.
   * @type {!HTMLInputElement}
   */
  get inputElement() {
    return this.$$('#pin-input');
  },

  /** Transfers focus to the input element. */
  focus: function() {
    this.$$('#pin-input').focus();
  },

  /**
   * Called when a keypad number has been tapped.
   * @param {!{target: !PaperButtonElement}} event
   * @private
   */
  onNumberTap_: function(event, detail) {
    var numberValue = event.target.getAttribute('value');
    this.value += numberValue;
  },

  /** Fires a submit event with the current PIN value. */
  firePinSubmitEvent_: function() {
    this.fire('submit', { pin: this.value });
  },

  /**
   * Fires an update event with the current PIN value. The event will only be
   * fired if the PIN value has actually changed.
   * @param {string} value
   * @param {string} previous
   */
  onPinValueChange_: function(value, previous) {
    if (value != previous)
      this.fire('pin-change', { pin: value });
  },

  /**
   * Called when the user wants to erase the last character of the entered
   * PIN value.
   * @private
   */
  onPinClear_: function() {
    this.value = this.value.substring(0, this.value.length - 1);
  },

  /**
   * Called when the user presses or touches the backspace button. Starts a
   * timer which starts an interval to repeatedly backspace the pin value until
   * the interval is cleared.
   * @param {Event} event The event object.
   * @private
   */
  onBackspacePointerDown_: function(event) {
    this.startAutoBackspaceId_ = setTimeout(function() {
        this.repeatBackspaceIntervalId_ = setInterval(
            this.onPinClear_.bind(this), REPEAT_BACKSPACE_DELAY_MS);
    }.bind(this), INITIAL_BACKSPACE_DELAY_MS);
  },

  /**
   * Helper function which clears the timer / interval ids and resets them.
   * @private
   */
  clearAndReset_: function() {
    clearInterval(this.repeatBackspaceIntervalId_);
    this.repeatBackspaceIntervalId_ = 0;
    clearTimeout(this.startAutoBackspaceId_);
    this.startAutoBackspaceId_ = 0;
  },

  /**
   * Called when the user exits the backspace button. Stops the interval
   * callback.
   * @param {Event} event The event object.
   * @private
   */
  onBackspacePointerOut_: function(event) {
    this.clearAndReset_();
  },

  /**
   * Called when the user unpresses or untouches the backspace button. Stops the
   * interval callback and fires a backspace event if there is no interval
   * running.
   * @param {Event} event The event object.
   * @private
   */
  onBackspacePointerUp_: function(event) {
    // If an interval has started, do not fire event on pointer up.
    if (!this.repeatBackspaceIntervalId_)
      this.onPinClear_();
    this.clearAndReset_();
  },

  /** Called when a key event is pressed while the input element has focus. */
  onInputKeyDown_: function(event) {
    // Up/down pressed, swallow the event to prevent the input value from
    // being incremented or decremented.
    if (event.keyCode == 38 || event.keyCode == 40) {
      event.preventDefault();
      return;
    }

    // Enter pressed.
    if (event.keyCode == 13) {
      this.firePinSubmitEvent_();
      event.preventDefault();
      return;
    }
  },

  /**
   * Keypress does not handle backspace but handles the char codes nicely, so we
   * have a seperate event to process the backspaces.
   * @param {Event} event Keydown Event object.
   * @private
   */
  onKeyDown_: function(event) {
    // Backspace pressed.
    if (event.keyCode == 8) {
      this.onPinClear_();
      event.preventDefault();
      return;
    }
  },

  /**
   * Called when a key press event is fired while the number button is focused.
   * Ideally we would want to pass focus back to the input element, but the we
   * cannot or the virtual keyboard will keep poping up.
   * @param {Event} event Keypress Event object.
   * @private
   */
  onKeyPress_: function(event) {
    // If the active element is the input element, the input element itself will
    // handle the keypresses, so we do not handle them here.
    if (this.shadowRoot.activeElement == this.inputElement)
      return;

    var code = event.keyCode;

    // Enter pressed.
    if (code == 13) {
      this.firePinSubmitEvent_();
      event.preventDefault();
      return;
    }

    // Space pressed. We want the old polymer function of space activating the
    // button with focus.
    if (code == 32) {
      // Check if target was a number button.
      if (event.target.hasAttribute('value')) {
        this.value += event.target.getAttribute('value');
        return;
      }
      // Check if target was backspace button.
      if (event.target.classList.contains('backspace-button')) {
        this.onPinClear_();
        return;
      }
    }

    this.value += String.fromCharCode(code);
  },

  /**
   * Disables the submit and backspace button if nothing is entered.
   * @param {string} value
   * @private
   */
  hasInput_: function(value) {
    return value.length > 0;
  },

  /**
   * Computes whether the input type for the pin input should be password or
   * numerical.
   * @param {boolean} enablePassword
   * @private
   */
  getInputType_: function(enablePassword) {
    return enablePassword ? 'password' : 'number';
  },

  /**
   * Computes the value of the pin input placeholder.
   * @param {boolean} enablePassword
   * @private
   */
  getInputPlaceholder_: function(enablePassword) {
    return enablePassword ? this.i18n('pinKeyboardPlaceholderPinPassword') :
                            this.i18n('pinKeyboardPlaceholderPin');
  },

  /**
   * Computes the direction of the pin input.
   * @param {string} password
   * @private
   */
  isInputRtl_: function(password) {
    // +password will convert a string to a number or to NaN if that's not
    // possible. Number.isInteger will verify the value is not a NaN and that it
    // does not contain decimals.
    // This heuristic will fail for inputs like '1.0'.
    //
    // Since we still support users entering their passwords through the PIN
    // keyboard, we swap the input box to rtl when we think it is a password
    // (just numbers), if the document direction is rtl.
    return (document.dir == 'rtl') && !Number.isInteger(+password);
  },
});
})();
