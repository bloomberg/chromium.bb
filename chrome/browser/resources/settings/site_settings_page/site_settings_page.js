// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

Polymer({
  is: 'settings-site-settings-page',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * An object to bind default values to (so they are not in the |this|
     * scope). The keys of this object are the values of the
     * settings.ContentSettingsTypes enum.
     * @private
     */
    default_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /** @private */
    enableSiteSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSiteSettings');
      },
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() { return loadTimeData.getBoolean('isGuest'); }
    },
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
    this.ALL_SITES = settings.ALL_SITES;

    var keys = Object.keys(settings.ContentSettingsTypes);
    for (var i = 0; i < keys.length; ++i) {
      var key = settings.ContentSettingsTypes[keys[i]];
      // Default labels are not applicable to USB and ZOOM.
      if (key == settings.ContentSettingsTypes.USB_DEVICES ||
          key == settings.ContentSettingsTypes.ZOOM_LEVELS)
        continue;
      // Some values are not available (and will DCHECK) in guest mode.
      if (this.isGuest_ &&
          key == settings.ContentSettingsTypes.PROTOCOL_HANDLERS) {
        continue;
      }
      this.updateDefaultValueLabel_(key);
    }

    this.addWebUIListener(
        'contentSettingCategoryChanged',
        this.updateDefaultValueLabel_.bind(this));
    this.addWebUIListener(
        'setHandlersEnabled',
        this.updateHandlersEnabled_.bind(this));
    this.browserProxy.observeProtocolHandlersEnabledState();
  },

  /**
   * @param {string} setting Value from settings.PermissionValues.
   * @param {string} enabled Non-block label ('feature X not allowed').
   * @param {string} disabled Block label (likely just, 'Blocked').
   * @param {?string} other Tristate value (maybe, 'session only').
   * @private
   */
  defaultSettingLabel_: function(setting, enabled, disabled, other) {
    if (setting == settings.PermissionValues.BLOCK)
      return disabled;
    if (setting == settings.PermissionValues.ALLOW)
      return enabled;
    if (other)
      return other;
    return enabled;
  },

  /**
   * @param {string} category The category to update.
   * @private
   */
  updateDefaultValueLabel_: function(category) {
    this.browserProxy.getDefaultValueForContentType(
        category).then(function(defaultValue) {
          this.set(
              'default_.' + Polymer.CaseMap.dashToCamelCase(category),
              defaultValue.setting);
        }.bind(this));
  },

  /**
   * The protocol handlers have a separate enabled/disabled notifier.
   * @param {boolean} enabled
   * @private
   */
  updateHandlersEnabled_: function(enabled) {
    var category = settings.ContentSettingsTypes.PROTOCOL_HANDLERS;
    this.set(
        'default_.' + Polymer.CaseMap.dashToCamelCase(category),
        enabled ?
            settings.PermissionValues.ALLOW :
            settings.PermissionValues.BLOCK);
  },

  /**
   * Navigate to the route specified in the event dataset.
   * @param {!Event} event The tap event.
   * @private
   */
  onTapNavigate_: function(event) {
    var dataSet = /** @type {{route: string}} */(event.currentTarget.dataset);
    settings.navigateTo(settings.Route[dataSet.route]);
  },
});
