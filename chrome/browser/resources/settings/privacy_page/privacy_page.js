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

  behaviors: [
    settings.RouteObserverBehavior,
<if expr="_google_chrome and not chromeos">
    WebUIListenerBehavior,
</if>
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
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,

<if expr="_google_chrome and not chromeos">
    /** @type {MetricsReporting} */
    metricsReporting_: Object,
</if>

    /** @private */
    showClearBrowsingDataDialog_: Boolean,
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;

<if expr="_google_chrome and not chromeos">
    var boundSetMetricsReporting = this.setMetricsReporting_.bind(this);
    this.addWebUIListener('metrics-reporting-change', boundSetMetricsReporting);

    var browserProxy = settings.PrivacyPageBrowserProxyImpl.getInstance();
    browserProxy.getMetricsReporting().then(boundSetMetricsReporting);
</if>
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showClearBrowsingDataDialog_ =
        settings.getCurrentRoute() == settings.Route.CLEAR_BROWSER_DATA;
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
    settings.navigateToPreviousRoute();
  },

<if expr="_google_chrome and not chromeos">
  /** @private */
  onMetricsReportingCheckboxTap_: function() {
    var browserProxy = settings.PrivacyPageBrowserProxyImpl.getInstance();
    var enabled = this.$.metricsReportingCheckbox.checked;
    browserProxy.setMetricsReportingEnabled(enabled);
  },

  /**
   * @param {!MetricsReporting} metricsReporting
   * @private
   */
  setMetricsReporting_: function(metricsReporting) {
    this.metricsReporting_ = metricsReporting;
  },
</if>
});
