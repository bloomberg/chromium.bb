// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
Polymer({
  is: 'all-sites',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Array of sites to display in the widget, grouped into their eTLD+1s.
     * @type {!Array<!SiteGroup>}
     */
    siteGroupList: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'contentSettingSitePermissionChanged', this.populateList_.bind(this));
    this.populateList_();
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_: function() {
    /** @type {!Array<settings.ContentSettingsTypes>} */
    const contentTypes = this.getCategoryList();
    // Make sure to include cookies, because All Sites handles data storage +
    // cookies as well as regular settings.ContentSettingsTypes.
    if (!contentTypes.includes(settings.ContentSettingsTypes.COOKIES))
      contentTypes.push(settings.ContentSettingsTypes.COOKIES);

    this.browserProxy_.getAllSites(contentTypes).then((response) => {
      this.siteGroupList = response;
    });
  },
});
