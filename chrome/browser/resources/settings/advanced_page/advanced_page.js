// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-advanced-page' is the settings page containing the advanced
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-advanced-page prefs="{{prefs}}">
 *      </cr-settings-advanced-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-advanced-page
 */
Polymer({
  is: 'cr-settings-advanced-page',

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
});
