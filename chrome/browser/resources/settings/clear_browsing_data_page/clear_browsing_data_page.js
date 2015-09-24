// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-clear-browsing-data-page' provides options to delete browsing
 * data that has been cached by chromium.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-clear-browsing-data-page prefs="{{prefs}}">
 *      </cr-settings-clear-browsing-data-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-privacy-page
 */
Polymer({
  is: 'cr-settings-clear-browsing-data-page',

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

    /**
     * List of options for the dropdown menu.
     * The order of entries in this array matches the
     * prefs.browser.clear_data.time_period.value enum.
     */
    clearFromOptions_: {
      readOnly: true,
      type: Array,
      value: [
        loadTimeData.getString('clearDataHour'),
        loadTimeData.getString('clearDataDay'),
        loadTimeData.getString('clearDataWeek'),
        loadTimeData.getString('clearData4Weeks'),
        loadTimeData.getString('clearDataEverything'),
      ],
    },
  },
});
