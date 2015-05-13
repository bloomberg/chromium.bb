// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-a11y-page' is the settings page containing accessibility
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-a11y-page prefs="{{prefs}}"></cr-settings-a11y-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-a11y-page
 */
Polymer({
  is: 'cr-settings-a11y-page',

  properties: {
    /**
     * Preferences state.
     *
     * @type {?CrSettingsPrefsElement}
     * @default null
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
      value: 'a11y',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('a11yPageTitle'); },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'accessibility',
      readOnly: true,
    },
  },

  /**
   * Called when the selected value of the autoclick dropdown changes.
   * TODO(jlklein): Replace with binding when paper-dropdown lands.
   * @private
   */
  autoclickSelectChanged_: function() {
    this.prefs.settings.settings.a11y.autoclick_delay_ms =
        this.$.autoclickDropdown.value;
  },
});
