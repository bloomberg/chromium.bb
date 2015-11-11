// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

/** @polymerBehavior */
var SiteSettingsBehaviorImpl = {
  /**
   * Returns whether the category default is set to enabled or not.
   * @param {number} category The category to check.
   * @return {boolean} True if the category default is set to enabled.
   * @protected
   */
  isCategoryAllowed: function(category) {
    var pref = this.getPref(this.computeCategoryPrefName(category));

    // FullScreen is Allow vs. Ask.
    if (category == settings.ContentSettingsTypes.FULLSCREEN)
      return pref.value != settings.PermissionValues.ASK;

    return pref.value != settings.PermissionValues.BLOCK;
  },

  /**
   * A utility function to compute the icon to use for the category.
   * @param {number} category The category to show the icon for.
   * @return {string} The id of the icon for the given category.
   * @protected
   */
  computeIconForContentCategory: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.COOKIES:
        return '';  // Haven't found a good cookies icon under iron-icons.
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'icons:input';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'icons:fullscreen';
      case settings.ContentSettingsTypes.POPUPS:
        return 'icons:open-in-new';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'communication:location-on';
      case settings.ContentSettingsTypes.NOTIFICATION:
        return 'social:notifications';
      case settings.ContentSettingsTypes.CAMERA:
        return 'av:videocam';
      case settings.ContentSettingsTypes.MIC:
        return 'av:mic';
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * A utility function to compute the title of the category.
   * @param {number} category The category to show the title for.
   * @return {string} The title for the given category.
   * @protected
   */
  computeTitleForContentCategory: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.COOKIES:
        return loadTimeData.getString('siteSettingsCookies');
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return loadTimeData.getString('siteSettingsJavascript');
      case settings.ContentSettingsTypes.FULLSCREEN:
        return loadTimeData.getString('siteSettingsFullscreen');
      case settings.ContentSettingsTypes.POPUPS:
        return loadTimeData.getString('siteSettingsPopups');
      case settings.ContentSettingsTypes.GEOLOCATION:
        return loadTimeData.getString('siteSettingsLocation');
      case settings.ContentSettingsTypes.NOTIFICATION:
        return loadTimeData.getString('siteSettingsNotifications');
      case settings.ContentSettingsTypes.CAMERA:
        return loadTimeData.getString('siteSettingsCamera');
      case settings.ContentSettingsTypes.MIC:
        return loadTimeData.getString('siteSettingsMic');
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * A utility function to compute the name of the pref for the category.
   * @param {number} category The category to find the pref name for.
   * @return {string} The pref name for the given category.
   * @protected
   */
  computeCategoryPrefName: function(category) {
    return 'profile.default_content_setting_values.' +
        this.computeCategorySuffix(category);
  },

  /**
   * A utility function to compute the name of the pref for the exceptions
   * for a given category.
   * @param {number} category The category to find the pref name for.
   * @return {string} The pref name for the given category exceptions.
   * @protected
   */
  computeCategoryExceptionsPrefName: function(category) {
    return 'profile.content_settings.exceptions.' +
        this.computeCategorySuffix(category);
  },

  /**
   * A utility function to convert the category enum into its text
   * representation, for use with prefs.
   * @param {number} category The category to find the pref name for.
   * @return {string} The pref name (suffix) for the given category.
   * @protected
   */
  computeCategorySuffix: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.COOKIES:
        return 'cookies';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'javascript';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'fullscreen';
      case settings.ContentSettingsTypes.POPUPS:
        return 'popups';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'geolocation';
      case settings.ContentSettingsTypes.NOTIFICATION:
        return 'notifications';
      case settings.ContentSettingsTypes.CAMERA:
        return 'media_stream_camera';
      case settings.ContentSettingsTypes.MIC:
        return 'media_stream_mic';
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * A utility function to compute the description for the category.
   * @param {number} category The category to show the description for.
   * @param {boolean} categoryEnabled The state of the global toggle.
   * @return {string} The category description.
   * @protected
   */
  computeCategoryDesc: function(category, categoryEnabled) {
    switch (category) {
      case settings.ContentSettingsTypes.JAVASCRIPT:
        // "Allowed (recommended)" vs "Blocked".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsAllowedRecommended') :
            loadTimeData.getString('siteSettingsBlocked');
      case settings.ContentSettingsTypes.POPUPS:
        // "Allowed" vs "Blocked (recommended)".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsAllowed') :
            loadTimeData.getString('siteSettingsBlockedRecommended');
      case settings.ContentSettingsTypes.NOTIFICATION:
        // "Ask before sending (recommended)" vs "Blocked".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsAskBeforeSending') :
            loadTimeData.getString('siteSettingsBlocked');
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.MIC:
        // "Ask before accessing (recommended)" vs "Blocked".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsAskBeforeAccessing') :
            loadTimeData.getString('siteSettingsBlocked');
      case settings.ContentSettingsTypes.FULLSCREEN:
        // "Allowed" vs. "Ask first (recommended)".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsAllowed') :
            loadTimeData.getString('siteSettingsAskFirstRecommended');
      case settings.ContentSettingsTypes.COOKIES:
        // "Allow sites to save and read cookie data" vs "Blocked".
        return categoryEnabled ?
            loadTimeData.getString('siteSettingsCookiesAllowed') :
            loadTimeData.getString('siteSettingsBlocked');
      default:
        assertNotReached();
        return '';
    }
  },
};

/** @polymerBehavior */
var SiteSettingsBehavior = [PrefsBehavior, SiteSettingsBehaviorImpl];
