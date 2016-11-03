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

    showRestart_: Boolean,
</if>

    /** @private */
    showClearBrowsingDataDialog_: Boolean,
  },

<if expr="_google_chrome">
  observers: [
    'updateSpellingService_(prefs.spellcheck.use_spelling_service.value)',
  ],
</if>

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;

<if expr="_google_chrome">
    this.updateSpellingService_();
</if>

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

  /**
   * This is a workaround to connect the remove all button to the subpage.
   * @private
   */
  onRemoveAllCookiesFromSite_: function() {
    var node = /** @type {?SiteDataDetailsSubpageElement} */(this.$$(
        'site-data-details-subpage'));
    if (node)
      node.removeAll();
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

  /** @private */
  onHelpTap_: function() {
    window.open(
        'https://support.google.com/chrome/?p=settings_manage_exceptions');
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
    if (this.metricsReporting_) {
      // TODO(dbeam): hide if changed back to the value Chrome started with.
      this.showRestart_ = true;
    }
    this.metricsReporting_ = metricsReporting;
  },

  /** @private */
  onRestartTap_: function() {
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
</if>

<if expr="_google_chrome">
  /** @private */
  updateSpellingService_: function() {
    this.$.spellingServiceToggleButton.checked =
        this.get('prefs.spellcheck.use_spelling_service.value');
  },

  /** @private */
  onUseSpellingServiceTap_: function() {
    this.set('prefs.spellcheck.use_spelling_service.value',
        this.$.spellingServiceToggleButton.checked);
  },
</if>

  /**
   * The sub-page title for the site or content settings.
   * @return {string}
   * @private
   */
  siteSettingsPageTitle_: function() {
    return loadTimeData.getBoolean('enableSiteSettings') ?
        loadTimeData.getString('siteSettings') :
        loadTimeData.getString('contentSettings');
  },
});
