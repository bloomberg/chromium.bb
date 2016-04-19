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

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * The category selected by the user.
     */
    categorySelected: {
      type: String,
      notify: true,
    },
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
    this.ALL_SITES = settings.ALL_SITES;

    // Look up the default value for each category and show it.
    this.setDefaultValue_(this.ContentSettingsTypes.COOKIES, '#cookies');
    this.setDefaultValue_(this.ContentSettingsTypes.GEOLOCATION,
        '#geolocation');
    this.setDefaultValue_(this.ContentSettingsTypes.CAMERA, '#camera');
    this.setDefaultValue_(this.ContentSettingsTypes.MIC, '#mic');
    this.setDefaultValue_(this.ContentSettingsTypes.JAVASCRIPT,
        '#javascript');
    this.setDefaultValue_(this.ContentSettingsTypes.POPUPS, '#popups');
    this.setDefaultValue_(this.ContentSettingsTypes.NOTIFICATIONS,
        '#notifications');
    this.setDefaultValue_(this.ContentSettingsTypes.IMAGES, '#images');

  },

  setDefaultValue_: function(category, id) {
    this.browserProxy.getDefaultValueForContentType(
        category).then(function(enabled) {
          var description = this.computeCategoryDesc(category, enabled, false);
          this.$$(id).innerText = description;
        }.bind(this));
  },

  /**
   * Handles selection of a single category and navigates to the details for
   * that category.
   * @param {!Event} event The tap event.
   */
  onTapCategory: function(event) {
    var category = parseInt(event.currentTarget.getAttribute('category'), 10);
    if (category == -1) {
      this.currentRoute = {
        page: this.currentRoute.page,
        section: 'privacy',
        subpage: ['site-settings', 'all-sites'],
      };
    } else {
      this.categorySelected = this.computeCategoryTextId(category);
      this.currentRoute = {
        page: this.currentRoute.page,
        section: 'privacy',
        subpage: ['site-settings', 'site-settings-category-' +
            this.categorySelected],
      };
    }
  },
});
