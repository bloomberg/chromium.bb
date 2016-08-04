// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-radio-button',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    name: {
      type: String,
      notify: true,
    },

    /** @private */
    controlled_: {
      type: Boolean,
      computed: 'computeControlled_(pref)',
      reflectToAttribute: true,
    },
  },

  /**
   * @param {chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether the button is disabled.
   * @private
   */
  computeControlled_: function(pref) {
    var pref = /** @type {!chrome.settingsPrivate.PrefObject} */(this.pref);
    return this.isPrefPolicyControlled(pref);
  },

  /**
   * @param {boolean} controlled
   * @param {string} name
   * @param {chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  showIndicator_: function(controlled, name, pref) {
    return controlled && name == Settings.PrefUtil.prefToString(pref);
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
});
