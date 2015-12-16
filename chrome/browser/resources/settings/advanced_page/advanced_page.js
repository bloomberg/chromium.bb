// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-advanced-page' is the settings page containing the advanced
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-advanced-page prefs="{{prefs}}">
 *      </settings-advanced-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-advanced-page
 */
Polymer({
  is: 'settings-advanced-page',

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
  },

  behaviors: [SettingsPageVisibility],
});
