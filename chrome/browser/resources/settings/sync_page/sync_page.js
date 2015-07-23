// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-sync-page' is the settings page containing sync settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-sync-page></cr-settings-sync-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-sync-page
 */
Polymer({
  is: 'cr-settings-sync-page',

  properties: {
    /**
     * Route for the page.
     */
    route: {
      type: String,
      value: ''
    },

    /**
     * Whether the page is a subpage.
     * TODO(khorimoto): Make this a subpage once the "People" full page has
     * landed, since this is supposed to be that page's subpage.
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
      value: 'sync',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() { return loadTimeData.getString('syncPageTitle'); },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'notification:sync',
      readOnly: true,
    },
  },
});
