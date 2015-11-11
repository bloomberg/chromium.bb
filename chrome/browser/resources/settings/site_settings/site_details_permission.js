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
     * The ID of the category this widget is displaying data for.
     */
    category: Number,

    /**
     * The origin, which this permission affects.
     */
    origin: {
      type: String,
      observer: 'initialize_',
    },

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

  initialize_: function() {
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

  resetPermission: function() {
    var pref = this.getPref(
        this.computeCategoryExceptionsPrefName(this.category));
    delete pref.value[this.origin + ',' + this.origin];
    delete pref.value[this.origin + ',*'];
    this.setPrefValue(
        this.computeCategoryExceptionsPrefName(this.category), pref.value);
    this.$.details.hidden = true;
  },
});
