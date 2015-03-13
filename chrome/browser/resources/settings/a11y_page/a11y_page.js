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
 *    <core-animated-pages>
 *      <cr-settings-a11y-page prefs="{{prefs}}"></cr-settings-a11y-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-a11y-page
 */
Polymer('cr-settings-a11y-page', {
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
     * @default 'a11y'
     */
    PAGE_ID: 'a11y',

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type string
     * @default 'Accessibility'
     */
    pageTitle: 'Accessibility',

    /**
     * Name of the 'core-icon' to show.
     *
     * @attribute icon
     * @type string
     * @default 'accessibility'
     */
    icon: 'accessibility',
  },
});
