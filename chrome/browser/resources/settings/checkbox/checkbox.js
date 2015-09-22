// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-settings-checkbox` is a checkbox that controls a supplied preference.
 *
 * Example:
 *      <cr-settings-checkbox pref="{{prefs.settings.enableFoo}}"
 *          label="Enable foo setting." subLabel="(bar also)">
 *      </cr-settings-checkbox>
 *
 * @element cr-settings-checkbox
 */
Polymer({
  is: 'cr-settings-checkbox',

  properties: {
    /**
     * The boolean preference object to control.
     * @type {?chrome.settingsPrivate.PrefObject}
     */
    pref: {
      type: Object,
      notify: true,
    },

    /** Whether the checkbox should represent the inverted value. */
    inverted: {
      type: Boolean,
      value: false,
    },

    /** Whether the checkbox is checked. */
    checked: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'checkedChanged_',
      reflectToAttribute: true
    },

    /** Disabled property for the element. */
    disabled: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true
    },

    /** Checkbox label. */
    label: {
      type: String,
      value: '',
    },

    /** Additional sub-label for the checkbox. */
    subLabel: {
      type: String,
      value: '',
    },
  },

  observers: [
    'prefValueChanged_(pref.value)'
  ],

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.checkbox, ['change']);
  },

  /**
   * Polymer observer for pref.value.
   * @param {*} prefValue
   * @private
   */
  prefValueChanged_: function(prefValue) {
    this.checked = this.getNewValue_(prefValue);
  },

  /**
   * Polymer observer for checked.
   * @private
   */
  checkedChanged_: function() {
    this.set('pref.value', this.getNewValue_(this.checked));
  },

  /**
   * @param {*} value
   * @return {boolean} The value as a boolean, inverted if |inverted| is true.
   * @private
   */
  getNewValue_: function(value) {
    return this.inverted ? !value : !!value;
  },

  /**
   * @param {boolean} disabled
   * @param {?chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether the checkbox should be disabled.
   * @private
   */
  checkboxDisabled_: function(disabled, pref) {
    return disabled || (!!pref &&
                        pref.policyEnforcement ==
                            chrome.settingsPrivate.PolicyEnforcement.ENFORCED);
  },
});
