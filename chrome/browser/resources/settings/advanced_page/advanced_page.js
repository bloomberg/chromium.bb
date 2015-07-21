// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-advanced-page' is the settings page containing the advanced
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-advanced-page prefs="{{prefs}}">
 *      </cr-settings-advanced-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-advanced-page
 */
Polymer({
  is: 'cr-settings-advanced-page',

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
      value: 'advanced',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('advancedPageTitle'); },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'menu',
      readOnly: true,
    },
  },
});
