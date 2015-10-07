// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the settings page containing accessibility
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-a11y-page prefs="{{prefs}}"></settings-a11y-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-a11y-page
 */
Polymer({
  is: 'settings-a11y-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  onMoreFeaturesTap_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },
});
