// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-location-page' is the settings page for location access.
 *
 * Example:
 *
 *      <cr-settings-location-page prefs="{{prefs}}">
 *      </cr-settings-location-page>
 *      ... other pages ...
 *
 * @group Chrome Settings Elements
 * @element cr-settings-location-page
 */
Polymer({
  is: 'cr-settings-location-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: true,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'location',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: '',
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'communication:location-on',
      readOnly: true,
    },

    /**
     * Array of objects with url members.
     */
    block: {
      type: Array,
    },

    /**
     * Array of objects with url members.
     */
    allow: {
      type: Array,
    },
  },

  ready: function() {
    this.block = [];
    this.allow = [];
  },

  getTitleAndCount_: function(title, count) {
    return loadTimeData.getStringF(
        'titleAndCount', loadTimeData.getString(title), count);
  },
});
