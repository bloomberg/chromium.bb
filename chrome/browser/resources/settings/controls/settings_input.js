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
    pref: {
      observer: 'prefChanged_'
    },

    /* The current value of the input, reflected to/from |pref|. */
    value: {
      type: String,
      value: '',
      notify: true,
    },

    /* Set to true to disable editing the input. */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    canTab: Boolean,

    /* Properties for paper-input. This is not strictly necessary.
     * Though it does define the types for the closure compiler. */
    errorMessage: { type: String },
    label: { type: String },
    noLabelFloat: { type: Boolean, value: false },
    pattern: { type: String },
    readonly: { type: Boolean, value: false },
    required: { type: Boolean, value: false },
    stopKeyboardEventPropagation: { type: Boolean, value: false },
    type: { type: String },
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

    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      this.value = this.pref.value.toString();
    } else {
      assert(this.pref.type == chrome.settingsPrivate.PrefType.STRING ||
             this.pref.type == chrome.settingsPrivate.PrefType.URL);
      this.value = /** @type {string} */(this.pref.value);
    }
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
    if (!this.pref)
      return;

    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      if (!this.value) {
        // Ignore empty input field and restore value.
        this.value = this.pref.value.toString();
        return;
      }
      var n = parseInt(this.value, 10);
      if (isNaN(n)) {
        console.error('Bad value for numerical pref: ' + this.value);
        return;
      }
      this.set('pref.value', n);
    } else {
      assert(this.pref.type == chrome.settingsPrivate.PrefType.STRING ||
             this.pref.type == chrome.settingsPrivate.PrefType.URL);
      this.set('pref.value', this.value);
    }
  },

  /**
   * @param {boolean} disabled
   * @return {boolean} Whether the element should be disabled.
   * @private
   */
  isDisabled_: function(disabled) {
    return disabled || this.isPrefPolicyControlled();
  },
});
