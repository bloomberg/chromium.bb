// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

/** @polymerBehavior */
var SiteSettingsBehaviorImpl = {
  properties: {
    /**
     * The ID of the category this element is displaying data for.
     * See site_settings/constants.js for possible values.
     */
    category: {
      type: Number,
    },
  },

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
   * Re-sets the category permission for a given origin.
   * @param {string} origin The origin to change the permission for.
   * @param {number} category The category permission to change.
   * @protected
   */
  resetCategoryPermissionForOrigin: function(origin, category) {
    var pref = JSON.parse(JSON.stringify(this.getPref(
        this.computeCategoryExceptionsPrefName(category))));
    delete pref.value[origin + ',' + origin];
    delete pref.value[origin + ',*'];
    this.setPrefValue(
        this.computeCategoryExceptionsPrefName(category), pref.value);
  },

  /**
   * Sets the category permission for a given origin.
   * @param {string} origin The origin to change the permission for.
   * @param {number} value What value to set the permission to.
   * @param {number} category The category permission to change.
   * @protected
   */
  setCategoryPermissionForOrigin: function(origin, value, category) {
    var pref = JSON.parse(JSON.stringify(this.getPref(
        this.computeCategoryExceptionsPrefName(category))));
    var key1 = origin + ',' + origin;
    var key2 = origin + ',*';
    if (pref.value[key1] != undefined)
      pref.value[key1].setting = value;
    if (pref.value[key2] != undefined)
      pref.value[key2].setting = value;
    this.setPrefValue(
        this.computeCategoryExceptionsPrefName(category), pref.value);
  },

  /**
   * A utility function to lookup a category name from its enum.
   * @param {number} category The category ID to look up.
   * @return {string} The category found or blank string if not found.
   * @protected
   */
  computeCategoryTextId: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.CAMERA:
        return 'camera';
      case settings.ContentSettingsTypes.COOKIES:
        return 'cookies';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'fullscreen';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'location';
      case settings.ContentSettingsTypes.IMAGES:
        return 'images';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'javascript';
      case settings.ContentSettingsTypes.MIC:
        return 'microphone';
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return 'notifications';
      case settings.ContentSettingsTypes.POPUPS:
        return 'popups';
      default:
        return '';
    }
  },

  /**
   * A utility function to compute the icon to use for the category.
   * @param {number} category The category to show the icon for.
   * @return {string} The id of the icon for the given category.
   * @protected
   */
  computeIconForContentCategory: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.CAMERA:
        return 'av:videocam';
      case settings.ContentSettingsTypes.COOKIES:
        return 'md-settings-icons:cookie';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'icons:fullscreen';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'communication:location-on';
      case settings.ContentSettingsTypes.IMAGES:
        return 'image:photo';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'icons:input';
      case settings.ContentSettingsTypes.MIC:
        return 'av:mic';
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return 'social:notifications';
      case settings.ContentSettingsTypes.POPUPS:
        return 'icons:open-in-new';
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
      case settings.ContentSettingsTypes.CAMERA:
        return loadTimeData.getString('siteSettingsCamera');
      case settings.ContentSettingsTypes.COOKIES:
        return loadTimeData.getString('siteSettingsCookies');
      case settings.ContentSettingsTypes.FULLSCREEN:
        return loadTimeData.getString('siteSettingsFullscreen');
      case settings.ContentSettingsTypes.GEOLOCATION:
        return loadTimeData.getString('siteSettingsLocation');
      case settings.ContentSettingsTypes.IMAGES:
        return loadTimeData.getString('siteSettingsImages');
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return loadTimeData.getString('siteSettingsJavascript');
      case settings.ContentSettingsTypes.MIC:
        return loadTimeData.getString('siteSettingsMic');
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return loadTimeData.getString('siteSettingsNotifications');
      case settings.ContentSettingsTypes.POPUPS:
        return loadTimeData.getString('siteSettingsPopups');
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
      case settings.ContentSettingsTypes.CAMERA:
        return 'media_stream_camera';
      case settings.ContentSettingsTypes.COOKIES:
        return 'cookies';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'fullscreen';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'geolocation';
      case settings.ContentSettingsTypes.IMAGES:
        return 'images';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'javascript';
      case settings.ContentSettingsTypes.MIC:
        return 'media_stream_mic';
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return 'notifications';
      case settings.ContentSettingsTypes.POPUPS:
        return 'popups';
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * A utility function to compute the description for the category.
   * @param {number} category The category to show the description for.
   * @param {boolean} categoryEnabled The state of the global toggle.
   * @param {boolean} showRecommendation Whether to show the '(recommended)'
   *     label prefix.
   * @return {string} The category description.
   * @protected
   */
  computeCategoryDesc: function(category, categoryEnabled, showRecommendation) {
    switch (category) {
      case settings.ContentSettingsTypes.JAVASCRIPT:
        // "Allowed (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAllowedRecommended') :
            loadTimeData.getString('siteSettingsAllowed');
      case settings.ContentSettingsTypes.POPUPS:
        // "Allowed" vs "Blocked (recommended)".
        if (categoryEnabled) {
          return loadTimeData.getString('siteSettingsAllowed');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsBlockedRecommended') :
            loadTimeData.getString('siteSettingsBlocked');
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        // "Ask before sending (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAskBeforeSendingRecommended') :
            loadTimeData.getString('siteSettingsAskBeforeSending');
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.MIC:
        // "Ask before accessing (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString(
                'siteSettingsAskBeforeAccessingRecommended') :
            loadTimeData.getString('siteSettingsAskBeforeAccessing');
      case settings.ContentSettingsTypes.FULLSCREEN:
        // "Allowed" vs. "Ask first (recommended)".
        if (categoryEnabled) {
          return loadTimeData.getString('siteSettingsAllowed');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAskFirstRecommended') :
            loadTimeData.getString('siteSettingsAskFirst');
      case settings.ContentSettingsTypes.COOKIES:
        // "Allow sites to save and read cookie data" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsCookiesAllowedRecommended') :
            loadTimeData.getString('siteSettingsCookiesAllowed');
      case settings.ContentSettingsTypes.IMAGES:
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsDontShowImages');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsShowAllRecommended') :
            loadTimeData.getString('siteSettingsShowAll');
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * A utility function to compute the category given the description.
   * @param {string} description The category description to look up.
   * @return {number} category The category id to return.
   * @protected
   */
  computeCategoryFromDesc: function(description) {
    for (var type in settings.ContentSettingsTypes) {
      if (description == this.computeTitleForContentCategory(
          settings.ContentSettingsTypes[type])) {
        return settings.ContentSettingsTypes[type];
      }
    }
    assertNotReached();
    return 0;
  },
};

/** @polymerBehavior */
var SiteSettingsBehavior = [PrefsBehavior, SiteSettingsBehaviorImpl];
