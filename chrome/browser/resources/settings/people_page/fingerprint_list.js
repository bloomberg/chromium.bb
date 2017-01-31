// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * The max number of fingerprints this list can hold.
 * @const {number}
 */
var MAX_NUMBER_FINGERPRINTS_ALLOWED = 5;

Polymer({
  is: 'settings-fingerprint-list',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The list of fingerprint objects.
     * @private {!Array<string>}
     */
    fingerprints_: {
      type: Array,
      value: function() {
        return [];
      }
    }
  },

  /**
   * Adds a fingerprint with a default name.
   * @private
   */
  onAddFingerprint_: function() {
    // Determines what the newly added fingerprint's name should be.
    // TODO(sammiequon): Add fingerprint using private API once it is ready.

    for (var i = 1; i <= MAX_NUMBER_FINGERPRINTS_ALLOWED; ++i) {
      var fingerprintName = this.i18n('lockScreenFingerprintNewName', i);
      if (!this.fingerprints_.includes(fingerprintName)) {
        this.push('fingerprints_', fingerprintName);
        break;
      }
    }
  },

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @private
   */
  onFingerprintDelete_: function(e) {
    // TODO(sammiequon): Remove fingerprint using private API once it is ready.
    this.splice('fingerprints_', e.model.index, 1);
  },

  /**
   * Returns the text to be displayed for the add fingerprint button.
   * @return {string}
   * @private
   */
  getFingerprintButtonText_: function() {
    if (this.canAddNewFingerprint_())
      return this.i18n('lockScreenAddFingerprint');

    return this.i18n('lockScreenCannotAddFingerprint',
                     MAX_NUMBER_FINGERPRINTS_ALLOWED);
  },

  /**
   * Checks whether another fingerprint can be added.
   * @return {boolean}
   * @private
   */
  canAddNewFingerprint_: function() {
    return this.fingerprints_.length < MAX_NUMBER_FINGERPRINTS_ALLOWED;
  }
});
})();
