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
    this.addAllSitesCategory_();
    this.addCategory_(settings.ContentSettingsTypes.COOKIES);
    this.addCategory_(settings.ContentSettingsTypes.GEOLOCATION);
    this.addCategory_(settings.ContentSettingsTypes.CAMERA);
    this.addCategory_(settings.ContentSettingsTypes.MIC);
    this.addCategory_(settings.ContentSettingsTypes.JAVASCRIPT);
    this.addCategory_(settings.ContentSettingsTypes.POPUPS);
    this.addCategory_(settings.ContentSettingsTypes.FULLSCREEN);
    this.addCategory_(settings.ContentSettingsTypes.NOTIFICATIONS);
    this.addCategory_(settings.ContentSettingsTypes.IMAGES);
  },

  /**
   * Adds the All Sites category to the page.
   * @private
   */
  addAllSitesCategory_: function() {
      var description = loadTimeData.getString('siteSettingsCategoryAllSites');
      this.addCategoryImpl_(-1, 'list', description, '');
  },

  /**
   * Adds a single category to the page.
   * @param {number} category The category to add.
   * @private
   */
  addCategory_: function(category) {
    var icon = this.computeIconForContentCategory(category);
    var title = this.computeTitleForContentCategory(category);
    var prefsProxy = settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    prefsProxy.getDefaultValueForContentType(
        category).then(function(enabled) {
          var description = this.computeCategoryDesc(category, enabled, false);
          this.addCategoryImpl_(category, icon, title, description);
        }.bind(this));
  },

  /**
   * Constructs the actual HTML elements for the category.
   * @param {number} category The category to add.
   * @param {string} icon The icon to show with it.
   * @param {string} title The title to show for the category.
   * @param {string} defaultValue The default value (permission) for the
   *     category.
   * @private
   */
  addCategoryImpl_: function(category, icon, title, defaultValue) {
    var root = this.$.list;
    var paperIcon = document.createElement('paper-icon-item');
    paperIcon.addEventListener('tap', this.onTapCategory.bind(this, category));

    var ironIcon = document.createElement('iron-icon');
    ironIcon.setAttribute('icon', icon);
    ironIcon.setAttribute('item-icon', '');

    var description = document.createElement('div');
    description.setAttribute('class', 'flex');
    description.appendChild(document.createTextNode(title));
    var setting = document.createElement('div');
    setting.setAttribute('class', 'option-value');

    setting.appendChild(document.createTextNode(defaultValue));

    paperIcon.appendChild(ironIcon);
    paperIcon.appendChild(description);
    paperIcon.appendChild(setting);
    root.appendChild(paperIcon);
  },

  /**
   * Handles selection of a single category and navigates to the details for
   * that category.
   * @param {number} category The category selected by the user.
   * @param {!Event} event The tap event.
   */
  onTapCategory: function(category, event) {
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
