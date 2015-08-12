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
     * @type {?PrefObject}
     */
    pref: {
      type: Object,
      notify: true
    },

    inverted: {
      type: Boolean,
      value: false
    },

    checked: {
      type: Boolean,
      value: false,
      observer: 'checkedChanged_'
    },

    label: {
      type: String,
      value: '',
    },

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

  /** @override */
  attached: function() {
    // HACK(dschuyler): paper-checkbox 1.0.6 will hide the label
    // if the content is empty.
    // TODO(dschuyler): rework settings checkbox to use content
    // rather than spans.
    this.$.checkbox.$.checkbox.$.checkboxLabel.hidden = false;
  },

  /** @private */
  prefValueChanged_: function(prefValue) {
    // prefValue is initially undefined when Polymer initializes pref.
    if (prefValue !== undefined) {
      this.checked = this.getNewValue_(prefValue);
    }
  },

  /** @private */
  checkedChanged_: function() {
    if (this.pref) {
      this.pref.value = this.getNewValue_(this.checked);
    }
  },

  /** @private */
  getNewValue_: function(val) {
    return this.inverted ? !val : val;
  }
});
