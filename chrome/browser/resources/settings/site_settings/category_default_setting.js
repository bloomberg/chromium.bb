// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-default-setting' is the polymer element for showing a certain
 * category under Site Settings.
 */
Polymer({
  is: 'category-default-setting',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Represents the state of the main toggle shown for the category. For
     * example, the Location category can be set to Block/Ask so false, in that
     * case, represents Block and true represents Ask.
     */
    categoryEnabled: Boolean,

    /**
     * The description to be shown next to the slider.
     * @private
     */
    sliderDescription_: String,

    /**
     * A sub-option toggle is used to represent tri-state categories.
     * @private
     */
    subOptionEnabled_: {
      type: Boolean,
      value: true,
    },
  },

  observers: [
    'onCategoryChanged_(category)',
  ],

  ready: function() {
    this.addWebUIListener('contentSettingCategoryChanged',
        this.defaultValueForCategoryChanged_.bind(this));
  },

  /**
   * Called when the default value for a category has been changed.
   * @param {number} category The category that changed.
   * @private
   */
  defaultValueForCategoryChanged_: function(category) {
    if (category == this.category)
      this.onCategoryChanged_();
  },

  /**
   * A handler for changing the default permission value for a content type.
   * @private
   */
  onChangePermissionControl_: function() {
    switch (this.category) {
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
      case settings.ContentSettingsTypes.IMAGES:
      case settings.ContentSettingsTypes.JAVASCRIPT:
      case settings.ContentSettingsTypes.KEYGEN:
      case settings.ContentSettingsTypes.POPUPS:
      case settings.ContentSettingsTypes.PROTOCOL_HANDLERS:
        // "Allowed" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ?
                settings.PermissionValues.ALLOW :
                settings.PermissionValues.BLOCK);
        break;
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.MIC:
      case settings.ContentSettingsTypes.NOTIFICATIONS:
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        // "Ask" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ?
                settings.PermissionValues.ASK :
                settings.PermissionValues.BLOCK);
        break;
      case settings.ContentSettingsTypes.COOKIES:
        // This category is tri-state: "Allow", "Block", "Keep data until
        // browser quits".
        var value = settings.PermissionValues.BLOCK;
        if (this.categoryEnabled) {
          value = this.subOptionEnabled_ ?
              settings.PermissionValues.SESSION_ONLY :
              settings.PermissionValues.ALLOW;
        }
        this.browserProxy.setDefaultValueForContentType(this.category, value);
        break;
      case settings.ContentSettingsTypes.PLUGINS:
        // This category is tri-state: "Allow", "Block", "Ask before running".
        var value = settings.PermissionValues.BLOCK;
        if (this.categoryEnabled) {
          value = this.subOptionEnabled_ ?
              settings.PermissionValues.IMPORTANT_CONTENT :
              settings.PermissionValues.ALLOW;
        }
        this.browserProxy.setDefaultValueForContentType(this.category, value);
        break;
      default:
        assertNotReached('Invalid category: ' + this.category);
    }
  },

  /**
   * Handles changes to the category pref and the |category| member variable.
   * @private
   */
  onCategoryChanged_: function() {
    this.browserProxy
      .getDefaultValueForContentType(this.category)
        .then(function(defaultValue) {
          var setting = defaultValue.setting;
          this.categoryEnabled = this.computeIsSettingEnabled(setting);

          // Flash only shows ALLOW or BLOCK descriptions on the slider.
          var sliderSetting = setting;
          if (this.category == settings.ContentSettingsTypes.PLUGINS &&
              setting == settings.PermissionValues.IMPORTANT_CONTENT) {
            sliderSetting = settings.PermissionValues.ALLOW;
          } else if (
              this.category == settings.ContentSettingsTypes.COOKIES &&
              setting == settings.PermissionValues.SESSION_ONLY) {
            sliderSetting = settings.PermissionValues.ALLOW;
          }
          this.sliderDescription_ =
              this.computeCategoryDesc(this.category, sliderSetting, true);

          if (this.category == settings.ContentSettingsTypes.PLUGINS) {
            // The checkbox should only be cleared when the Flash setting
            // is explicitly set to ALLOW.
            if (setting == settings.PermissionValues.ALLOW)
              this.subOptionEnabled_ = false;
            if (setting == settings.PermissionValues.IMPORTANT_CONTENT)
              this.subOptionEnabled_ = true;
          } else if (
              this.category == settings.ContentSettingsTypes.COOKIES) {
            if (setting == settings.PermissionValues.ALLOW)
              this.subOptionEnabled_ = false;
            else if (setting == settings.PermissionValues.SESSION_ONLY)
              this.subOptionEnabled_ = true;
          }
        }.bind(this));
  },
});
