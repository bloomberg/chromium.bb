// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission' handles showing the state of one permission, such
 * as Geolocation, for a given origin.
 *
 * Example:
 *
 *      <site-details-permission prefs="{{prefs}}">
 *      </site-details-permission>
 *      ... other pages ...
 *
 * @group Chrome Settings Elements
 * @element site-details-permission
 */
Polymer({
  is: 'site-details-permission',

  behaviors: [PrefsBehavior, SiteSettingsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The origin, which this permission affects.
     */
    origin: String,

    i18n_: {
      readOnly: true,
      type: Object,
      value: function() {
        return {
          allowAction: loadTimeData.getString('siteSettingsActionAllow'),
          blockAction: loadTimeData.getString('siteSettingsActionBlock'),
        };
      },
    },
  },

  observers: [
    'initialize_(' +
        'prefs.profile.content_settings.exceptions.*, category, origin)',
  ],

  initialize_: function() {
    this.$.details.hidden = true;
    if (this.get('prefs.' +
        this.computeCategoryExceptionsPrefName(this.category)) === undefined)
      return;

    var pref = this.getPref(
        this.computeCategoryExceptionsPrefName(this.category));
    var originPref = pref.value[this.origin + ',*'];
    if (originPref === undefined)
      originPref = pref.value[this.origin + ',' + this.origin];
    if (originPref === undefined)
      return;

    if (/** @type {{setting: number}} */(originPref.setting) ==
        settings.PermissionValues.ALLOW) {
      this.$.permission.selected = 0;
      this.$.details.hidden = false;
    } else if (originPref.setting == settings.PermissionValues.BLOCK) {
      this.$.permission.selected = 1;
      this.$.details.hidden = false;
    }
  },

  /**
   * Resets the category permission for this origin.
   */
  resetPermission: function() {
    this.resetCategoryPermissionForOrigin(this.origin, this.category);
    this.$.details.hidden = true;
  },

  /**
   * Handles the category permission changing for this origin.
   * @param {!{target: !{selectedItem: !{innerText: string}}}} event
   */
  onPermissionMenuIronSelect_: function(event) {
    var action = event.target.selectedItem.innerText;
    var value = (action == this.i18n_.allowAction) ?
        settings.PermissionValues.ALLOW :
        settings.PermissionValues.BLOCK;
    this.setCategoryPermissionForOrigin(this.origin, value, this.category);
  },
});
