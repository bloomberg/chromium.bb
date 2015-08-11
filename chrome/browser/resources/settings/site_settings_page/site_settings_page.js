// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-site-settings-page prefs="{{prefs}}">
 *      </cr-settings-site-settings-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-site-settings-page
 */
Polymer({
  is: 'cr-settings-site-settings-page',

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
      value: false,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'site-settings',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('siteSettingsPageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'lock',
      readOnly: true,
    },
  },
});
