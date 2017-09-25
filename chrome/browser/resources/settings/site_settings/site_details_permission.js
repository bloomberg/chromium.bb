// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission' handles showing the state of one permission, such
 * as Geolocation, for a given origin.
 */
Polymer({
  is: 'site-details-permission',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior, I18nBehavior],

  properties: {
    /**
     * The site that this widget is showing details for.
     * @type {RawSiteException}
     */
    site: Object,

    /**
     * The default setting for this permission category.
     * @type {settings.ContentSetting}
     * @private
     */
    defaultSetting_: String,
  },

  observers: ['siteChanged_(site)'],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'contentSettingCategoryChanged',
        this.onDefaultSettingChanged_.bind(this));
  },

  /**
   * Updates the drop-down value after |site| has changed.
   * @param {!RawSiteException} site The site to display.
   * @private
   */
  siteChanged_: function(site) {
    if (site.source == settings.SiteSettingSource.DEFAULT) {
      this.defaultSetting_ = site.setting;
      this.$.permission.value = settings.ContentSetting.DEFAULT;
    } else {
      // The default setting is unknown, so consult the C++ backend for it.
      this.updateDefaultPermission_(site);
      this.$.permission.value = site.setting;
    }

    if (this.isNonDefaultAsk_(site.setting, site.source)) {
      assert(
          this.$.permission.disabled,
          'The \'Ask\' entry is for display-only and cannot be set by the ' +
              'user.');
      assert(
          this.$.permission.value == settings.ContentSetting.ASK,
          '\'Ask\' should only show up when it\'s currently selected.');
    }
  },

  /**
   * Updates the default permission setting for this permission category.
   * @param {!RawSiteException} site The site to display.
   * @private
   */
  updateDefaultPermission_: function(site) {
    this.browserProxy.getDefaultValueForContentType(this.category)
        .then((defaultValue) => {
          this.defaultSetting_ = defaultValue.setting;
        });
  },

  /**
   * Handles the category permission changing for this origin.
   * @param {!settings.ContentSettingsTypes} category The permission category
   *     that has changed default permission.
   * @private
   */
  onDefaultSettingChanged_: function(category) {
    if (category == this.category)
      this.updateDefaultPermission_(this.site);
  },

  /**
   * Handles the category permission changing for this origin.
   * @private
   */
  onPermissionSelectionChange_: function() {
    this.browserProxy.setOriginPermissions(
        this.site.origin, [this.category], this.$.permission.value);
  },

  /**
   * Updates the string used for this permission category's default setting.
   * @param {!settings.ContentSetting} defaultSetting Value of the default
   *     setting for this permission category.
   * @param {string} askString 'Ask' label, e.g. 'Ask (default)'.
   * @param {string} allowString 'Allow' label, e.g. 'Allow (default)'.
   * @param {string} blockString 'Block' label, e.g. 'Blocked (default)'.
   * @private
   */
  defaultSettingString_: function(
      defaultSetting, askString, allowString, blockString) {
    if (defaultSetting == settings.ContentSetting.ASK ||
        defaultSetting == settings.ContentSetting.IMPORTANT_CONTENT) {
      return askString;
    } else if (defaultSetting == settings.ContentSetting.ALLOW) {
      return allowString;
    } else if (defaultSetting == settings.ContentSetting.BLOCK) {
      return blockString;
    }
    assertNotReached(
        `No string for ${this.category}'s default of ${defaultSetting}`);
  },

  /**
   * Returns true if there's a string to display that describes the source of
   * this permission's setting. Currently, this only gets called when
   * |this.site| is updated.
   * @param {!settings.SiteSettingSource} source The source of the permission.
   * @private
   */
  hasPermissionSourceString_: function(source) {
    return (
        source != settings.SiteSettingSource.DEFAULT &&
        source != settings.SiteSettingSource.PREFERENCE);
  },

  /**
   * Checks if there's a permission source string to display, and returns the
   * class name to apply to permissions if so.
   * @return {string} CSS class applied when there is an additional description
   *     string.
   * @private
   */
  permissionSourceStringClass_: function(source) {
    return this.hasPermissionSourceString_(source) ? 'two-line' : '';
  },

  /**
   * Returns true if this permission's source is controlled by the user.
   * @return {boolean}
   * @private
   */
  isPermissionUserControlled_: function(source) {
    // Users are able override embargo.
    return !this.hasPermissionSourceString_(source) ||
        source == settings.SiteSettingSource.EMBARGO;
  },

  /**
   * Returns true if the permission is set to a non-default 'ask'. Currently,
   * this only gets called when |this.site| is updated.
   * @param {!settings.ContentSetting} setting The setting of the permission.
   * @param {!settings.SiteSettingSource} source The source of the permission.
   * @private
   */
  isNonDefaultAsk_: function(setting, source) {
    if (setting != settings.ContentSetting.ASK ||
        source == settings.SiteSettingSource.DEFAULT) {
      return false;
    }

    assert(
        source == settings.SiteSettingSource.EXTENSION ||
            source == settings.SiteSettingSource.POLICY,
        'Only extensions or enterprise policy can change the setting to ASK.');
    return true;
  },

  /**
   * Updates the string used to describe the source of this permission setting.
   * Currently, this only gets called when |this.site| is updated.
   * @param {!settings.SiteSettingSource} source The source of the permission.
   * @param {!string} embargoString
   * @param {!string} insecureOriginString
   * @param {!string} killSwitchString
   * @param {!string} extensionAllowString
   * @param {!string} extensionBlockString
   * @param {!string} extensionAskString
   * @param {!string} policyAllowString
   * @param {!string} policyBlockString
   * @param {!string} policyAskString
   * @private
   */
  permissionSourceString_: function(
      source, embargoString, insecureOriginString, killSwitchString,
      extensionAllowString, extensionBlockString, extensionAskString,
      policyAllowString, policyBlockString, policyAskString) {

    /** @type {Object<!settings.ContentSetting, string>} */
    var extensionStrings = {};
    extensionStrings[settings.ContentSetting.ALLOW] = extensionAllowString;
    extensionStrings[settings.ContentSetting.BLOCK] = extensionBlockString;
    extensionStrings[settings.ContentSetting.ASK] = extensionAskString;

    /** @type {Object<!settings.ContentSetting, string>} */
    var policyStrings = {};
    policyStrings[settings.ContentSetting.ALLOW] = policyAllowString;
    policyStrings[settings.ContentSetting.BLOCK] = policyBlockString;
    policyStrings[settings.ContentSetting.ASK] = policyAskString;

    if (source == settings.SiteSettingSource.DRM_DISABLED) {
      assert(
          settings.ContentSetting.BLOCK == this.site.setting,
          'If DRM is disabled, Protected Content must be blocked.');
      assert(
          settings.ContentSettingsTypes.PROTECTED_CONTENT == this.category,
          'The DRM disabled source only applies to Protected Content.');
      return this.i18nAdvanced('siteSettingsSourceDrmDisabled', {
        substitutions:
            [settings.routes.SITE_SETTINGS_PROTECTED_CONTENT.getAbsolutePath()]
      });
    } else if (source == settings.SiteSettingSource.EMBARGO) {
      assert(
          settings.ContentSetting.BLOCK == this.site.setting,
          'Embargo is only used to block permissions.');
      return embargoString;
    } else if (source == settings.SiteSettingSource.EXTENSION) {
      return extensionStrings[this.site.setting];
    } else if (source == settings.SiteSettingSource.INSECURE_ORIGIN) {
      assert(
          settings.ContentSetting.BLOCK == this.site.setting,
          'Permissions can only be blocked due to insecure origins.');
      return insecureOriginString;
    } else if (source == settings.SiteSettingSource.KILL_SWITCH) {
      assert(
          settings.ContentSetting.BLOCK == this.site.setting,
          'The permissions kill switch can only be used to block permissions.');
      return killSwitchString;
    } else if (source == settings.SiteSettingSource.POLICY) {
      return policyStrings[this.site.setting];
    } else if (
        source == settings.SiteSettingSource.DEFAULT ||
        source == settings.SiteSettingSource.PREFERENCE) {
      return '';
    }
    assertNotReached(
        `No string for ${this.category} setting source '${source}'`);
  },
});
