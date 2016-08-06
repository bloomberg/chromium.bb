// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
Polymer({
  is: 'settings-privacy-page',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showClearBrowsingDataDialog_: Boolean,

    /**
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showClearBrowsingDataDialog_ =
        settings.getCurrentRoute().dialog == 'clear-browsing-data';
  },

  /** @private */
  onManageCertificatesTap_: function() {
<if expr="use_nss_certs">
    settings.navigateTo(settings.Route.CERTIFICATES);
</if>
<if expr="is_win or is_macosx">
    settings.PrivacyPageBrowserProxyImpl.getInstance().
        showManageSSLCertificates();
</if>
  },

  /** @private */
  onSiteSettingsTap_: function() {
    settings.navigateTo(settings.Route.SITE_SETTINGS);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    settings.navigateTo(settings.Route.CLEAR_BROWSER_DATA);
  },

  /** @private */
  onDialogClosed_: function() {
    settings.navigateTo(settings.Route.PRIVACY);
  },
});
