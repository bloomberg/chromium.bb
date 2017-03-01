// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-radio-button',

  behaviors: [PrefControlBehavior],

  properties: {
    name: {
      type: String,
      notify: true,
    },

    /** @private */
    controlled_: {
      type: Boolean,
      computed: 'computeControlled_(pref.*)',
      reflectToAttribute: true,
    },
  },

  listeners: {
    'focus': 'onFocus_',
  },

  /**
   * @return {boolean} Whether the button is disabled.
   * @private
   */
  computeControlled_: function() {
    return this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @param {boolean} controlled
   * @param {string} name
   * @return {boolean}
   * @private
   */
  showIndicator_: function(controlled, name) {
    return controlled &&
        name == Settings.PrefUtil.prefToString(assert(this.pref));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-radio-button on-tap="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /** Focuses the internal radio button when the row is selected. */
  onFocus_: function() {
    this.$.radioButton.focus();
  },
});
