// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-input` is a single-line text field for user input associated
 * with a pref value.
 */
Polymer({
  is: 'settings-input',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /**
     * The preference object to control.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     * @override
     */
    pref: {observer: 'prefChanged_'},

    /* The current value of the input, reflected to/from |pref|. */
    value: {
      type: String,
      value: '',
      notify: true,
    },

    /* Set to true to disable editing the input. */
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    canTab: Boolean,

    invalid: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /* Properties for paper-input. This is not strictly necessary.
     * Though it does define the types for the closure compiler. */
    errorMessage: {type: String},
    label: {type: String},
  },

  /**
   * Focuses the 'input' element.
   */
  focus: function() {
    this.$.input.focus();
  },

  /**
   * Polymer changed observer for |pref|.
   * @private
   */
  prefChanged_: function() {
    if (!this.pref)
      return;

    // Ignore updates while the input is focused so that user input is not
    // overwritten.
    if (this.$.input.focused)
      return;

    this.setInputValueFromPref_();
  },

  /** @private */
  setInputValueFromPref_: function() {
    assert(this.pref.type == chrome.settingsPrivate.PrefType.URL);
    this.value = /** @type {string} */ (this.pref.value);
  },

  /**
   * Gets a tab index for this control if it can be tabbed to.
   * @param {boolean} canTab
   * @return {number}
   * @private
   */
  getTabindex_: function(canTab) {
    return canTab ? 0 : -1;
  },

  /**
   * Change event handler for paper-input. Updates the pref value.
   * settings-input uses the change event because it is fired by the Enter key.
   * @private
   */
  onChange_: function() {
    if (this.invalid) {
      this.resetValue_();
      return;
    }

    assert(this.pref.type == chrome.settingsPrivate.PrefType.URL);
    this.set('pref.value', this.value);
  },

  /** @private */
  resetValue_: function() {
    this.invalid = false;
    this.setInputValueFromPref_();
    this.$.input.blur();
  },

  /**
   * Keydown handler to specify enter-key and escape-key interactions.
   * @param {!Event} event
   * @private
   */
  onKeydown_: function(event) {
    // If pressed enter when input is invalid, do not trigger on-change.
    if (event.key == 'Enter' && this.invalid) {
      event.preventDefault();
      return;
    }

    if (event.key != 'Escape')
      return;

    this.resetValue_();
  },

  /**
   * @param {boolean} disabled
   * @return {boolean} Whether the element should be disabled.
   * @private
   */
  isDisabled_: function(disabled) {
    return disabled || this.isPrefEnforced();
  },
});
