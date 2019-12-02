// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
(function() {

Polymer({
  is: 'settings-personalization-options',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,

    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    // <if expr="not chromeos">
    /** @private {Array<!settings.StoredAccount>} */
    storedAccounts_: Object,
    // </if>

    /** @private */
    userSignedIn_: {
      type: Boolean,
      computed: 'computeUserSignedIn_(syncStatus, storedAccounts_)',
    },

    /** @private */
    passwordsLeakDetectionAvailable_: {
      type: Boolean,
      computed: 'computePasswordsLeakDetectionAvailable_(prefs.*)',
    },

    /** @private */
    passwordsLeakDetectionEnabled_: {
      type: Boolean,
      value: loadTimeData.getBoolean('passwordsLeakDetectionEnabled'),
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

    /** @private */
    showRestart_: Boolean,
    // </if>

    /** @private */
    privacySettingsRedesignEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('privacySettingsRedesignEnabled');
      },
    },
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    // <if expr="_google_chrome and not chromeos">
    const setMetricsReportingPref = this.setMetricsReportingPref_.bind(this);
    this.addWebUIListener('metrics-reporting-change', setMetricsReportingPref);
    this.browserProxy_.getMetricsReporting().then(setMetricsReportingPref);
    // </if>
    // <if expr="not chromeos">
    const storedAccountsChanged = storedAccounts => this.storedAccounts_ =
        storedAccounts;
    const syncBrowserProxy = settings.SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    // Even though we already set checked="[[getCheckedLeakDetection_(...)]]"
    // in the DOM, this might be overridden within prefValueChanged_ of
    // SettingsBooleanControlBehaviorImpl which gets invoked once we navigate to
    // sync_page.html. Re-computing the checked value here once fixes this
    // problem.
    this.$.passwordsLeakDetectionCheckbox.checked =
        this.getCheckedLeakDetection_();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeUserSignedIn_: function() {
    return (!!this.syncStatus && !!this.syncStatus.signedIn) ?
        !this.syncStatus.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  computePasswordsLeakDetectionAvailable_: function() {
    return !!this.getPref('profile.password_manager_leak_detection').value &&
        !!this.getPref('safebrowsing.enabled').value;
  },

  /**
   * @return {boolean}
   * @private
   */
  getCheckedLeakDetection_: function() {
    return this.userSignedIn_ && this.passwordsLeakDetectionAvailable_;
  },

  /**
   * @return {string}
   * @private
   */
  getPasswordsLeakDetectionSubLabel_: function() {
    if (!this.userSignedIn_ && this.passwordsLeakDetectionAvailable_) {
      return this.i18n('passwordsLeakDetectionSignedOutEnabledDescription');
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledLeakDetection_: function() {
    return !this.userSignedIn_ || !this.getPref('safebrowsing.enabled').value;
  },

  /**
   * @return {boolean}
   * @private
   */
  getCheckedExtendedSafeBrowsing_: function() {
    return !!this.getPref('safebrowsing.enabled').value &&
        !!this.getPref('safebrowsing.scout_reporting_enabled').value;
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledExtendedSafeBrowsing_: function() {
    return !this.getPref('safebrowsing.enabled').value;
  },

  // <if expr="_google_chrome and not chromeos">
  /** @private */
  onMetricsReportingChange_: function() {
    const enabled = this.$.metricsReportingControl.checked;
    this.browserProxy_.setMetricsReportingEnabled(enabled);
  },

  /**
   * @param {!MetricsReporting} metricsReporting
   * @private
   */
  setMetricsReportingPref_: function(metricsReporting) {
    const hadPreviousPref = this.metricsReportingPref_.value !== undefined;
    const pref = {
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
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPreviousPref) {
      this.showRestart_ = true;
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRestartTap_: function(e) {
    e.stopPropagation();
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
  // </if>

  // <if expr="_google_chrome">
  /**
   * @param {!Event} event
   * @private
   */
  onUseSpellingServiceToggle_: function(event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if (event.target.checked) {
      this.setPrefValue('browser.enable_spellchecking', true);
    }
  },
  // </if>

  /**
   * @return {boolean}
   * @private
   */
  showSpellCheckControl_: function() {
    return (
        !!this.prefs.spellcheck &&
        /** @type {!Array<string>} */
        (this.prefs.spellcheck.dictionaries.value).length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowDriveSuggest_: function() {
    return loadTimeData.getBoolean('driveSuggestAvailable') &&
        !!this.syncStatus && !!this.syncStatus.signedIn &&
        this.syncStatus.statusAction !== settings.StatusAction.REAUTHENTICATE;
  },
});
})();
