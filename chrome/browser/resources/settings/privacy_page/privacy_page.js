// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
(function() {

/**
 * Must be kept in sync with the C++ enum of the same name.
 * @enum {number}
 */
var NetworkPredictionOptions = {ALWAYS: 0, WIFI_ONLY: 1, NEVER: 2, DEFAULT: 1};

Polymer({
  is: 'settings-privacy-page',

  behaviors: [
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
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

    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    // <if expr="_google_chrome and not chromeos">
    // TODO(dbeam): make a virtual.* pref namespace and set/get this normally
    // (but handled differently in C++).
    /** @private {chrome.settingsPrivate.PrefObject} */
    metricsReportingPref_: {
      type: Object,
      value: function() {
        // TODO(dbeam): this is basically only to appease PrefControlBehavior.
        // Maybe add a no-validate attribute instead? This makes little sense.
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    showRestart_: Boolean,
    // </if>

    /** @private {chrome.settingsPrivate.PrefObject} */
    safeBrowsingExtendedReportingPref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private */
    showClearBrowsingDataDialog_: Boolean,

    /** @private */
    showDoNotTrackDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used for HTML bindings. This is defined as a property rather than within
     * the ready callback, because the value needs to be available before
     * local DOM initialization - otherwise, the toggle has unexpected behavior.
     * @private
     */
    networkPredictionEnum_: {
      type: Object,
      value: NetworkPredictionOptions,
    },

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        // <if expr="use_nss_certs">
        map.set(
            settings.Route.CERTIFICATES.path,
            '#manageCertificates .subpage-arrow');
        // </if>
        map.set(
            settings.Route.SITE_SETTINGS.path,
            '#site-settings-subpage-trigger .subpage-arrow');
        return map;
      },
    },
  },

  listeners: {
    'doNotTrackDialogIf.dom-change': 'onDoNotTrackDomChange_',
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;

    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    // <if expr="_google_chrome and not chromeos">
    var setMetricsReportingPref = this.setMetricsReportingPref_.bind(this);
    this.addWebUIListener('metrics-reporting-change', setMetricsReportingPref);
    this.browserProxy_.getMetricsReporting().then(setMetricsReportingPref);
    // </if>

    var setSber = this.setSafeBrowsingExtendedReporting_.bind(this);
    this.addWebUIListener('safe-browsing-extended-reporting-change', setSber);
    this.browserProxy_.getSafeBrowsingExtendedReporting().then(setSber);
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showClearBrowsingDataDialog_ =
        settings.getCurrentRoute() == settings.Route.CLEAR_BROWSER_DATA;
  },

  /**
   * @param {Event} event
   * @private
   */
  onDoNotTrackDomChange_: function(event) {
    if (this.showDoNotTrackDialog_)
      this.maybeShowDoNotTrackDialog_();
  },

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   * @param {Event} event
   * @private
   */
  onDoNotTrackChange_: function(event) {
    var target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!target.checked) {
      // Always allow disabling the pref.
      target.sendPrefChange();
      return;
    }
    this.showDoNotTrackDialog_ = true;
    // If the dialog has already been stamped, show it. Otherwise it will be
    // shown in onDomChange_.
    this.maybeShowDoNotTrackDialog_();
  },

  /** @private */
  maybeShowDoNotTrackDialog_: function() {
    var dialog = this.$$('#confirmDoNotTrackDialog');
    if (dialog && !dialog.open)
      dialog.showModal();
  },

  /** @private */
  closeDoNotTrackDialog_: function() {
    this.$$('#confirmDoNotTrackDialog').close();
    this.showDoNotTrackDialog_ = false;
  },

  /** @private */
  onDoNotTrackDialogClosed_: function() {
    cr.ui.focusWithoutInk(this.$.doNotTrack);
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onDoNotTrackDialogConfirm_: function() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
        .sendPrefChange();
    this.closeDoNotTrackDialog_();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onDoNotTrackDialogCancel_: function() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
        .resetToPrefValue();
    this.closeDoNotTrackDialog_();
  },

  /** @private */
  onManageCertificatesTap_: function() {
    // <if expr="use_nss_certs">
    settings.navigateTo(settings.Route.CERTIFICATES);
    // </if>
    // <if expr="is_win or is_macosx">
    this.browserProxy_.showManageSSLCertificates();
    // </if>
  },

  /**
   * This is a workaround to connect the remove all button to the subpage.
   * @private
   */
  onRemoveAllCookiesFromSite_: function() {
    var node = /** @type {?SiteDataDetailsSubpageElement} */ (
        this.$$('site-data-details-subpage'));
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
    cr.ui.focusWithoutInk(assert(this.$.clearBrowsingDataTrigger));
  },

  /** @private */
  onSberChange_: function() {
    var enabled = this.$.safeBrowsingExtendedReportingControl.checked;
    this.browserProxy_.setSafeBrowsingExtendedReportingEnabled(enabled);
  },

  // <if expr="_google_chrome and not chromeos">
  /** @private */
  onMetricsReportingChange_: function() {
    var enabled = this.$.metricsReportingControl.checked;
    this.browserProxy_.setMetricsReportingEnabled(enabled);
  },

  /**
   * @param {!MetricsReporting} metricsReporting
   * @private
   */
  setMetricsReportingPref_: function(metricsReporting) {
    var hadPreviousPref = this.metricsReportingPref_.value !== undefined;
    var pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: metricsReporting.enabled,
    };
    if (metricsReporting.managed) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    // Ignore the next change because it will happen when we set the pref.
    this.metricsReportingPref_ = pref;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed)
      this.showRestart_ = false;
    else if (hadPreviousPref)
      this.showRestart_ = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  onRestartTap_: function(e) {
    e.stopPropagation();
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
  // </if>

  /**
   * @param {boolean} enabled Whether reporting is enabled or not.
   * @private
   */
  setSafeBrowsingExtendedReporting_: function(enabled) {
    // Ignore the next change because it will happen when we set the pref.
    this.safeBrowsingExtendedReportingPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: enabled,
    };
  },

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

  /** @private */
  getProtectedContentLabel_: function(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnable') :
                   this.i18n('siteSettingsBlocked');
  },

  /** @private */
  getProtectedContentIdentifiersLabel_: function(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnableIdentifiers') :
                   this.i18n('siteSettingsBlocked');
  },
});
})();
