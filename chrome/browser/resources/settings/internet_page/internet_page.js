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
 *      <cr-settings-internet-page prefs="{{prefs}}">
 *      </cr-settings-internet-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-internet-page
 */
Polymer('cr-settings-internet-page', {
  publish: {
    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const string
     */
    PAGE_ID: 'internet',

    /**
     * Route for the page.
     *
     * @attribute route
     * @type string
     * @default ''
     */
    route: '',

    /**
     * Whether the page is a subpage.
     *
     * @attribute subpage
     * @type boolean
     * @default false
     */
    subpage: false,

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type string
     */
    pageTitle: loadTimeData.getString('internetPageTitle'),

    /**
     * Name of the 'core-icon' to show. TODO(stevenjb): Update this with the
     * icon for the active internet connection.
     *
     * @attribute icon
     * @type string
     * @default 'settings-ethernet'
     */
    icon: 'settings-ethernet',
  },
});
