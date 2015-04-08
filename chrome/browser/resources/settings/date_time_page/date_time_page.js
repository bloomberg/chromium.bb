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
Polymer('cr-settings-date-time-page', {
  publish: {
    /**
     * Preferences state.
     *
     * @attribute prefs
     * @type CrSettingsPrefsElement
     * @default null
     */
    prefs: null,

    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const string
     * @default 'date-time'
     */
    PAGE_ID: 'date-time',

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type string
     * @default 'Date & Time'
     */
    pageTitle: 'Date & Time',

    /**
     * Name of the 'core-icon' to show.
     *
     * @attribute icon
     * @type string
     * @default 'device:access-time'
     */
    icon: 'device:access-time',
  },
});
