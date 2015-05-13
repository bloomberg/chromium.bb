// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-date-time-page' is the settings page containing date-time
 * settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <cr-settings-date-time-page prefs="{{prefs}}">
 *      </cr-settings-date-time-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-date-time-page
 */
Polymer({
  is: 'cr-settings-date-time-page',

  properties: {
    /**
     * Preferences state.
     * @type {?CrSettingsPrefsElement}
     */
    prefs: {
      type: Object,
      notify: true
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
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'date-time',
      readOnly: true
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('dateTimePageTitle'); }
    },

    /**
     * Name of the 'core-icon' to show.
     */
    icon: {
      type: String,
      value: 'device:access-time',
      readOnly: true
    },
  },
});
