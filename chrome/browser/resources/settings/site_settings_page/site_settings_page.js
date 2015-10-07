// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-site-settings-page prefs="{{prefs}}">
 *      </settings-site-settings-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-site-settings-page
 */
Polymer({
  is: 'settings-site-settings-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
