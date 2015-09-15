// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-privacy-page prefs="{{prefs}}">
 *      </cr-settings-privacy-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-privacy-page
 */
Polymer({
  is: 'cr-settings-privacy-page',

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

  /** @private */
  onManageCertificatesTap_: function() {
    this.$.pages.setSubpageChain(['manage-certificates']);
  },

  /** @private */
  onSiteSettingsTap_: function() {
    this.$.pages.setSubpageChain(['site-settings']);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    // TODO(dschuyler)
  },
});
