// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-button',

  behaviors: [
    CrPolicyIndicatorBehavior,
    CrPolicyPrefBehavior,
    PrefControlBehavior,
  ],

  properties: {
    endJustified: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    controlled_: {
      type: Boolean,
      computed: 'computeControlled_(pref.*)',
      reflectToAttribute: true,
    },
  },

  /**
   * @return {boolean} Whether the button is disabled.
   * @private
   */
  computeControlled_: function() {
    return this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-button on-tap="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether to show a controlled by indicator.
   * @private
   */
  showIndicator_: function(pref) {
    if (!pref.controlledBy || !pref.enforcement)
      return false;
    var indicator = this.getIndicatorType(pref.controlledBy, pref.enforcement);
    return this.isIndicatorVisible(indicator);
  },
});
