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
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @type {!Map<string, string>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    var R = settings.Route;
    [[R.SITE_SETTINGS_COOKIES, 'cookies'],
     [R.SITE_SETTINGS_LOCATION, 'location'], [R.SITE_SETTINGS_CAMERA, 'camera'],
     [R.SITE_SETTINGS_MICROPHONE, 'microphone'],
     [R.SITE_SETTINGS_NOTIFICATIONS, 'notifications'],
     [R.SITE_SETTINGS_JAVASCRIPT, 'javascript'],
     [R.SITE_SETTINGS_FLASH, 'flash'], [R.SITE_SETTINGS_IMAGES, 'images'],
     [R.SITE_SETTINGS_POPUPS, 'popups'],
     [R.SITE_SETTINGS_BACKGROUND_SYNC, 'background-sync'],
     [R.SITE_SETTINGS_AUTOMATIC_DOWNLOADS, 'automatic-downloads'],
     [R.SITE_SETTINGS_UNSANDBOXED_PLUGINS, 'unsandboxed-plugins'],
     [R.SITE_SETTINGS_HANDLERS, 'protocol-handlers'],
     [R.SITE_SETTINGS_MIDI_DEVICES, 'midi-devices'],
     [R.SITE_SETTINGS_ADS, 'ads'], [R.SITE_SETTINGS_ZOOM_LEVELS, 'zoom-levels'],
     [R.SITE_SETTINGS_USB_DEVICES, 'usb-devices'],
     [R.SITE_SETTINGS_PDF_DOCUMENTS, 'pdf-documents'],
     [R.SITE_SETTINGS_PROTECTED_CONTENT, 'protected-content'],
    ].forEach(function(pair) {
      var route = pair[0];
      var id = pair[1];
      this.focusConfig.set(route.path, '* /deep/ #' + id + ' .subpage-arrow');
    }.bind(this));
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
        'setHandlersEnabled', this.updateHandlersEnabled_.bind(this));
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
    this.browserProxy.getDefaultValueForContentType(category).then(
        function(defaultValue) {
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
        enabled ? settings.PermissionValues.ALLOW :
                  settings.PermissionValues.BLOCK);
  },

  /**
   * Navigate to the route specified in the event dataset.
   * @param {!Event} event The tap event.
   * @private
   */
  onTapNavigate_: function(event) {
    var dataSet = /** @type {{route: string}} */ (event.currentTarget.dataset);
    settings.navigateTo(settings.Route[dataSet.route]);
  },
});
