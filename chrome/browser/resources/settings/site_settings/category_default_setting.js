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
    /** @private {chrome.settingsPrivate.PrefObject} */
    controlParams_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */({});
      },
    },

    /** @private {!DefaultContentSetting} */
    priorDefaultContentSetting_: {
      type: Object,
      value: function() {
        return /** @type {DefaultContentSetting} */({});
      },
    },

    /**
     * The description to be shown next to the slider.
     * @private
     */
    sliderDescription_: String,

    /**
     * Cookies and Flash settings have a sub-control that is used to mimic a
     * tri-state value.
     * @private {chrome.settingsPrivate.PrefObject}
     */
    subControlParams_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */({});
      },
    },

    /* Labels for the toggle on/off positions. */
    toggleOffLabel: String,
    toggleOnLabel: String,
  },

  observers: [
    'onCategoryChanged_(category)',
    'onChangePermissionControl_(category, controlParams_.value, ' +
        'subControlParams_.value)',
  ],

  ready: function() {
    this.addWebUIListener('contentSettingCategoryChanged',
        this.onCategoryChanged_.bind(this));
  },

  /** @return {boolean} */
  get categoryEnabled() {
    return !!assert(this.controlParams_).value;
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
      case settings.ContentSettingsTypes.MIDI_DEVICES:
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
          value = this.subControlParams_.value ?
              settings.PermissionValues.SESSION_ONLY :
              settings.PermissionValues.ALLOW;
        }
        this.browserProxy.setDefaultValueForContentType(this.category, value);
        break;
      case settings.ContentSettingsTypes.PLUGINS:
        // This category is tri-state: "Allow", "Block", "Ask before running".
        var value = settings.PermissionValues.BLOCK;
        if (this.categoryEnabled) {
          value = this.subControlParams_.value ?
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
   * Update the control parameter values from the content settings.
   * @param {!DefaultContentSetting} update
   * @private
   */
  updateControlParams_: function(update) {
    // Early out if there is no actual change.
    if (this.priorDefaultContentSetting_.setting == update.setting &&
        this.priorDefaultContentSetting_.source == update.source) {
      return;
    }
    this.priorDefaultContentSetting_ = update;

    var basePref = {
      'key': 'controlParams',
      'type': chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (update.source !== undefined &&
        update.source != ContentSettingProvider.PREFERENCE) {
      basePref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      basePref.controlledBy =
          update.source == ContentSettingProvider.EXTENSION ?
          chrome.settingsPrivate.ControlledBy.EXTENSION :
          chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    var prefValue = this.computeIsSettingEnabled(update.setting);
    // The controlParams_ must be replaced (rather than just value changes) so
    // that observers will be notified of the change.
    this.controlParams_ = /** @type {chrome.settingsPrivate.PrefObject} */(
        Object.assign({'value': prefValue}, basePref));

    var subPrefValue = false;
    if (this.category == settings.ContentSettingsTypes.PLUGINS ||
        this.category == settings.ContentSettingsTypes.COOKIES) {
      if (this.category == settings.ContentSettingsTypes.PLUGINS &&
          update.setting == settings.PermissionValues.IMPORTANT_CONTENT) {
        subPrefValue = true;
      } else if (this.category == settings.ContentSettingsTypes.COOKIES &&
          update.setting == settings.PermissionValues.SESSION_ONLY) {
        subPrefValue = true;
      }
    }
    // The subControlParams_ must be replaced (rather than just value changes)
    // so that observers will be notified of the change.
    this.subControlParams_ = /** @type {chrome.settingsPrivate.PrefObject} */(
        Object.assign({'value': subPrefValue}, basePref));
  },

  /**
   * Handles changes to the category pref and the |category| member variable.
   * @private
   */
  onCategoryChanged_: function() {
    this.browserProxy
        .getDefaultValueForContentType(
          this.category).then(function(defaultValue) {
            this.updateControlParams_(defaultValue);

            // Flash only shows ALLOW or BLOCK descriptions on the slider.
            var setting = defaultValue.setting;
            if (this.category == settings.ContentSettingsTypes.PLUGINS &&
                setting == settings.PermissionValues.IMPORTANT_CONTENT) {
              setting = settings.PermissionValues.ALLOW;
            } else if (
                this.category == settings.ContentSettingsTypes.COOKIES &&
                setting == settings.PermissionValues.SESSION_ONLY) {
              setting = settings.PermissionValues.ALLOW;
            }
            var categoryEnabled = setting != settings.PermissionValues.BLOCK;
            this.sliderDescription_ =
                categoryEnabled ? this.toggleOnLabel : this.toggleOffLabel;
          }.bind(this));
  },

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_: function() {
    return this.category == settings.ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  }
});
