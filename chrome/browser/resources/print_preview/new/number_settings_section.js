// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-number-settings-section',

  behaviors: [print_preview_new.InputBehavior],

  properties: {
    /** @private {string} */
    inputString_: {
      type: String,
      notify: true,
      observer: 'onInputChanged_',
    },

    /** @type {boolean} */
    inputValid: {
      type: Boolean,
      notify: true,
      value: true,
    },

    /** @type {string} */
    currentValue: {
      type: String,
      notify: true,
      observer: 'onCurrentValueChanged_',
    },

    defaultValue: String,

    maxValue: Number,

    minValue: Number,

    inputLabel: String,

    hintMessage: String,

    disabled: Boolean,
  },

  listeners: {
    'input-change': 'onInputChange_',
  },

  /** @return {!HTMLInputElement} The input field element for InputBehavior. */
  getInput: function() {
    return this.$.userValue;
  },

  /**
   * @param {!CustomEvent} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    this.inputString_ = /** @type {string} */ (e.detail);
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  getDisabled_: function() {
    return this.disabled && this.inputValid;
  },

  /**
   * @param {!KeyboardEvent} e The keyboard event
   */
  onKeydown_: function(e) {
    if (e.key == '.' || e.key == 'e' || e.key == '-')
      e.preventDefault();
  },

  /** @private */
  onBlur_: function() {
    if (this.inputString_ == '')
      this.set('inputString_', this.defaultValue);
    if (this.$.userValue.value == '')
      this.$.userValue.value = this.defaultValue;
  },

  /** @private */
  onInputChanged_: function() {
    this.inputValid = this.computeValid_();
    this.currentValue = this.inputString_;
  },

  /** @private */
  onCurrentValueChanged_: function() {
    this.inputString_ = this.currentValue;
    this.resetString();
  },

  /**
   * @return {boolean} Whether input value represented by inputString_ is
   *     valid.
   * @private
   */
  computeValid_: function() {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.userValue.value = this.inputString_;
    return this.$.userValue.validity.valid && this.inputString_ != '';
  },

  /**
   * @return {boolean} Whether error message should be hidden.
   * @private
   */
  hintHidden_: function() {
    return this.inputValid || this.inputString_ == '';
  },
});
