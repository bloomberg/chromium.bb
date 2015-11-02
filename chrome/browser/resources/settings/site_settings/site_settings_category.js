// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-settings-category' is the settings page for showing a certain
 * category under Site Settings.
 *
 * Example:
 *
 *   <site-settings-category prefs="{{prefs}}">
 *   </site-settings-category>
 *   ... other pages ...
 *
 * @group Chrome Settings Elements
 * @element site-settings-category
 */
Polymer({
  is: 'site-settings-category',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Represents the state of the main toggle shown for the category. For
     * example, the Location category can be set to Block/Ask so false, in that
     * case, represents Block and true represents Ask.
     */
    categoryEnabled: {
      type: Boolean,
    },

    /**
     * The ID of the category this widget is displaying data for.
     */
    category: {
      type: Number,
    },
  },

  observers: [
    'categoryPrefChanged_(prefs.profile.default_content_setting_values.*)',
  ],

  ready: function() {
    this.$.blockList.categorySubtype = settings.DefaultValues.BLOCK;
    this.$.allowList.categorySubtype = settings.DefaultValues.ALLOW;

    CrSettingsPrefs.initialized.then(function() {
      this.categoryEnabled = this.isCategoryAllowed(this.category);
    }.bind(this));
  },

  /**
   * A handler for flipping the toggle value.
   * @private
   */
  onToggleChange_: function(event) {
    assert(CrSettingsPrefs.isInitialized);

    switch (this.category) {
      case settings.ContentSettingsTypes.COOKIES:
      case settings.ContentSettingsTypes.JAVASCRIPT:
      case settings.ContentSettingsTypes.POPUPS:
        // "Allowed" vs "Blocked".
        this.setPrefValue(this.computeCategoryPrefName(this.category),
                          this.categoryEnabled ?
                              settings.DefaultValues.ALLOW :
                              settings.DefaultValues.BLOCK);
        break;
      case settings.ContentSettingsTypes.NOTIFICATION:
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.MIC:
        // "Ask" vs "Blocked".
        this.setPrefValue(this.computeCategoryPrefName(this.category),
                          this.categoryEnabled ?
                              settings.DefaultValues.ASK :
                              settings.DefaultValues.BLOCK);
        break;
      case settings.ContentSettingsTypes.FULLSCREEN:
        // "Allowed" vs. "Ask first".
        this.setPrefValue(this.computeCategoryPrefName(this.category),
                          this.categoryEnabled ?
                              settings.DefaultValues.ALLOW :
                              settings.DefaultValues.ASK);
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * Handles when the global toggle changes.
   * @private
   */
  categoryPrefChanged_: function() {
    this.categoryEnabled = this.isCategoryAllowed(this.category);
  },
});
