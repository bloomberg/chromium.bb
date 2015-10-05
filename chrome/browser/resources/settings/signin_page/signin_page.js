// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-signin-page' is the settings page containing sign-in settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-signin-page prefs="{{prefs}}"></settings-signin-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-signin-page
 */
Polymer({
  is: 'settings-signin-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  onSyncTap_: function() {
    this.$.pages.setSubpageChain(['sync']);
  },
});
