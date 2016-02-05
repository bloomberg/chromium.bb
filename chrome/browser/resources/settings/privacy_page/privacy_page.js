// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-privacy-page prefs="{{prefs}}">
 *      </settings-privacy-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-privacy-page
 */
Polymer({
  is: 'settings-privacy-page',

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

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
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
    this.$.pages.setSubpageChain(['clear-browsing-data']);
  },
});
