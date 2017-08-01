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

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

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

  observers: ['siteChanged_(site, category)'],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'contentSettingSitePermissionChanged',
        this.sitePermissionChanged_.bind(this));
  },

  /**
   * Returns true if the origins match, e.g. http://google.com and
   * http://[*.]google.com.
   * @param {string} left The first origin to compare.
   * @param {string} right The second origin to compare.
   * @return {boolean} True if the origins are the same.
   * @private
   */
  sameOrigin_: function(left, right) {
    return this.removePatternWildcard(left) ==
        this.removePatternWildcard(right);
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
      this.browserProxy.getDefaultValueForContentType(this.category)
          .then((defaultValue) => {
            this.defaultSetting_ = defaultValue.setting;
          });
      this.$.permission.value = site.setting;
    }

    // Handle non-default sources.
    if (site.source == settings.SiteSettingSource.DEFAULT ||
        site.source == settings.SiteSettingSource.PREFERENCE) {
      this.$.permissionItem.classList.remove('two-line');
      this.$.permission.disabled = false;
    } else {
      this.$.permissionItem.classList.add('two-line');
      // Users are able to override embargo, so leave enabled in that case.
      this.$.permission.disabled =
          site.source != settings.SiteSettingSource.EMBARGO;
    }
  },

  /**
   * Called when a site within a category has been changed.
   * @param {number} category The category that changed.
   * @param {string} origin The origin of the site that changed.
   * @param {string} embeddingOrigin The embedding origin of the site that
   *     changed.
   * @private
   */
  sitePermissionChanged_: function(category, origin, embeddingOrigin) {
    if (this.site === undefined)
      return;
    if (category != this.category)
      return;

    if (origin == '' ||
        (origin == this.site.origin &&
         embeddingOrigin == this.site.embeddingOrigin)) {
      this.siteChanged_(this.site);
    }
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
   * this permission's setting.
   * Note |source| is a subproperty of |this.site|, so this will only be called
   * when |this.site| is updated.
   * @param {!settings.SiteSettingSource} source The source of the permission.
   * @private
   */
  hasPermissionSourceString_: function(source) {
    return (
        source != settings.SiteSettingSource.DEFAULT &&
        source != settings.SiteSettingSource.PREFERENCE);
  },

  /**
   * Updates the string used to describe the source of this permission setting.
   * Note |source| is a subproperty of |this.site|, so this will only be called
   * when |this.site| is updated.
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

    var extensionStrings =
        /** @type {Object<!settings.ContentSetting, string>} */ {};
    extensionStrings[settings.ContentSetting.ALLOW] = extensionAllowString;
    extensionStrings[settings.ContentSetting.BLOCK] = extensionBlockString;
    extensionStrings[settings.ContentSetting.ASK] = extensionAskString;

    var policyStrings =
        /** @type {Object<!settings.ContentSetting, string>} */ {};
    policyStrings[settings.ContentSetting.ALLOW] = policyAllowString;
    policyStrings[settings.ContentSetting.BLOCK] = policyBlockString;
    policyStrings[settings.ContentSetting.ASK] = policyAskString;

    if (source == settings.SiteSettingSource.EMBARGO) {
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
