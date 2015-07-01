// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-internet-page' is the settings page containing internet
 * settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <cr-settings-internet-page prefs='{{prefs}}'>
 *      </cr-settings-internet-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-internet-page
 */
Polymer({
  is: 'cr-settings-internet-page',

  properties: {
    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'internet',
      readOnly: true
    },

    /**
     * Route for the page.
     */
    route: {
      type: String,
      value: ''
    },

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('internetPageTitle'); }
    },

    /**
     * Name of the 'core-icon' to show. TODO(stevenjb): Update this with the
     * icon for the active internet connection.
     */
    icon: {
      type: String,
      value: 'settings-ethernet',
      readOnly: true
    },
  },
});
