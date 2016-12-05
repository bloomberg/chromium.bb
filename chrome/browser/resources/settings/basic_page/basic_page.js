// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the basic settings.
 */
Polymer({
  is: 'settings-basic-page',

  behaviors: [SettingsPageVisibility, MainPageBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showAndroidApps: Boolean,

    /**
     * True if the basic page should currently display the reset profile banner.
     * @private {boolean}
     */
    showResetProfileBanner_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showResetProfileBanner');
      },
    },
  },

  /** @private */
  onResetDone_: function() {
    this.showResetProfileBanner_ = false;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowAndroidApps_: function() {
    var visibility = /** @type {boolean|undefined} */ (
        this.get('pageVisibility.androidApps'));
    return this.showAndroidApps && this.showPage(visibility);
  },
});
