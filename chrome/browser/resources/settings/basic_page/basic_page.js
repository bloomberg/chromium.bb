// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-basic-page' is the settings page containing the basic settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-basic-page prefs="{{prefs}}"></cr-settings-basic-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-basic-page
 */
Polymer({
  is: 'cr-settings-basic-page',

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
