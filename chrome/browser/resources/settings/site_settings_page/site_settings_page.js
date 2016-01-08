// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-site-settings-page prefs="{{prefs}}">
 *      </settings-site-settings-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-site-settings-page
 */
Polymer({
  is: 'settings-site-settings-page',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

  ready: function() {
    CrSettingsPrefs.initialized.then(function() {
      // TODO(finnur): Implement 'All Sites' list.
      this.addCategory(settings.ContentSettingsTypes.COOKIES);
      this.addCategory(settings.ContentSettingsTypes.GEOLOCATION);
      this.addCategory(settings.ContentSettingsTypes.CAMERA);
      this.addCategory(settings.ContentSettingsTypes.MIC);
      this.addCategory(settings.ContentSettingsTypes.JAVASCRIPT);
      this.addCategory(settings.ContentSettingsTypes.POPUPS);
      this.addCategory(settings.ContentSettingsTypes.FULLSCREEN);
      this.addCategory(settings.ContentSettingsTypes.NOTIFICATIONS);
      this.addCategory(settings.ContentSettingsTypes.IMAGES);
    }.bind(this));
  },

  /**
   * Adds a single category to the page.
   * @param {number} category The category to add.
   */
  addCategory: function(category) {
    var root = this.$.list;
    var paperIcon = document.createElement('paper-icon-item');
    paperIcon.addEventListener('tap', this.onTapCategory.bind(this));

    var ironIcon = document.createElement('iron-icon');
    ironIcon.setAttribute('icon', this.computeIconForContentCategory(category));
    ironIcon.setAttribute('item-icon', '');

    var description = document.createElement('div');
    description.setAttribute('class', 'flex');
    description.appendChild(
        document.createTextNode(this.computeTitleForContentCategory(category)));
    var setting = document.createElement('div');
    setting.setAttribute('class', 'option-value');

    setting.appendChild(document.createTextNode(
        this.computeCategoryDesc(
            category, this.isCategoryAllowed(category), false)));

    paperIcon.appendChild(ironIcon);
    paperIcon.appendChild(description);
    paperIcon.appendChild(setting);
    root.appendChild(paperIcon);
  },

  /**
   * Handles selection of a single category and navigates to the details for
   * that category.
   */
  onTapCategory: function(event) {
    var description = event.currentTarget.querySelector('.flex').innerText;
    var page = this.computeCategoryTextId(
        this.computeCategoryFromDesc(description));
    this.currentRoute = {
      page: this.currentRoute.page,
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-' + page],
    };
  },
});
