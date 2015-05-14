// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-downloads-page' is the settings page containing downloads
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-downloads-page prefs="{{prefs}}">
 *      </cr-settings-downloads-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-downloads-page
 */
Polymer({
  is: 'cr-settings-downloads-page',

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
      value: 'downloads',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('downloadsPageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'file-download',
      readOnly: true,
    },
  },

  /** @private */
  selectDownloadLocation_: function() {
    // TODO(orenb): Communicate with the C++ to actually display a folder
    // picker.
    this.$.downloadsPath.value = '/Downloads';
  },
});
