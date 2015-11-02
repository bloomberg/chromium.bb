// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common prefs behavior.
 */

/** @polymerBehavior */
var PrefsBehavior = {
  /**
   * Gets the pref at the given prefPath. Throws if the pref is not found.
   * @param {string} prefPath
   * @return {!chrome.settingsPrivate.PrefObject}
   * @protected
   */
  getPref: function(prefPath) {
    var pref = /** @type {!chrome.settingsPrivate.PrefObject} */(
        this.get(prefPath, this.prefs));
    assert(typeof pref != 'undefined', 'Pref is missing: ' + prefPath);
    return pref;
  },

  /**
   * Sets the value of the pref at the given prefPath. Throws if the pref is not
   * found.
   * @param {string} prefPath
   * @param {*} value
   * @protected
   */
  setPrefValue: function(prefPath, value) {
    this.getPref(prefPath);  // Ensures we throw if the pref is not found.
    this.set('prefs.' + prefPath + '.value', value);
  },
};
