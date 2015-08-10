// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-certificate-manager-page' is the settings page containing SSL
 * certificate settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-certificate-manager-page prefs="{{prefs}}">
 *      </cr-settings-certificate-manager-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-certificate-manager-page
 */
Polymer({
  is: 'cr-settings-certificate-manager-page',

  properties: {
    /**
     * Preferences state.
     * TODO(dschuyler) check whether this is necessary.
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
      value: 'certificate-manager',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('certificateManagerPageTitle');
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
