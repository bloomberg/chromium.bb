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
 */
Polymer({
  is: 'settings-privacy-page',

  behaviors: [
    I18nBehavior,
  ],

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

    /** @private */
    showClearBrowsingDataDialog_: Boolean,
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /** @private */
  onManageCertificatesTap_: function() {
<if expr="use_nss_certs">
    this.$.pages.setSubpageChain(['manage-certificates']);
</if>
<if expr="is_win or is_macosx">
    settings.PrivacyPageBrowserProxyImpl.getInstance().
      showManageSSLCertificates();
</if>
  },

  /** @private */
  onSiteSettingsTap_: function() {
    this.$.pages.setSubpageChain(['site-settings']);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    this.showClearBrowsingDataDialog_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronOverlayClosed_: function(event) {
    if (Polymer.dom(event).rootTarget.tagName == 'SETTINGS-DIALOG')
      this.showClearBrowsingDataDialog_ = false;
  },
});
