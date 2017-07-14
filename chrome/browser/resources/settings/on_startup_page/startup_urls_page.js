// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

Polymer({
  is: 'settings-startup-urls-page',

  behaviors: [CrScrollableBehavior, WebUIListenerBehavior],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,

    /** @private */
    showStartupUrlDialog_: Boolean,

    /** @private {?StartupPageInfo} */
    startupUrlDialogModel_: Object,

    /** @private {Object}*/
    lastFocused_: Object,

    /** @private {?NtpExtension} */
    ntpExtension_: Object,

    /**
     * Enum values for the 'session.restore_on_startup' preference.
     * @private {!Object<string, number>}
     */
    prefValues_: {
      readOnly: true,
      type: Object,
      value: {
        CONTINUE: 1,
        OPEN_NEW_TAB: 5,
        OPEN_SPECIFIC: 4,
      },
    },
  },

  /** @private {?settings.StartupUrlsPageBrowserProxy} */
  browserProxy_: null,

  /**
   * The element to return focus to, when the startup-url-dialog is closed.
   * @private {?HTMLElement}
   */
  startupUrlDialogAnchor_: null,

  /** @override */
  attached: function() {
    this.getNtpExtension_();
    this.addWebUIListener('update-ntp-extension', function(ntpExtension) {
      // Note that |ntpExtension| is empty if there is no NTP extension.
      this.ntpExtension_ = ntpExtension;
    }.bind(this));

    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();
    this.addWebUIListener('update-startup-pages', function(startupPages) {
      // If an "edit" URL dialog was open, close it, because the underlying page
      // might have just been removed (and model indices have changed anyway).
      if (this.startupUrlDialogModel_)
        this.destroyUrlDialog_();
      this.startupPages_ = startupPages;
      this.updateScrollableContents();
    }.bind(this));
    this.browserProxy_.loadStartupPages();

    this.addEventListener(settings.EDIT_STARTUP_URL_EVENT, function(event) {
      this.startupUrlDialogModel_ = event.detail.model;
      this.startupUrlDialogAnchor_ = event.detail.anchor;
      this.showStartupUrlDialog_ = true;
      event.stopPropagation();
    }.bind(this));
  },

  /** @private */
  getNtpExtension_: function() {
    settings.OnStartupBrowserProxyImpl.getInstance().getNtpExtension().then(
        function(ntpExtension) {
          this.ntpExtension_ = ntpExtension;
        }.bind(this));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAddPageTap_: function(e) {
    e.preventDefault();
    this.showStartupUrlDialog_ = true;
    this.startupUrlDialogAnchor_ =
        /** @type {!HTMLElement} */ (this.$$('#addPage a[is=action-link]'));
  },

  /** @private */
  destroyUrlDialog_: function() {
    this.showStartupUrlDialog_ = false;
    this.startupUrlDialogModel_ = null;
    if (this.startupUrlDialogAnchor_) {
      cr.ui.focusWithoutInk(assert(this.startupUrlDialogAnchor_));
      this.startupUrlDialogAnchor_ = null;
    }
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    this.browserProxy_.useCurrentPages();
  },

  /**
   * @return {boolean} Whether "Add new page" and "Use current pages" are
   *     allowed.
   * @private
   */
  shouldAllowUrlsEdit_: function() {
    return this.get('prefs.session.startup_urls.enforcement') !=
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @param {?NtpExtension} ntpExtension
   * @param {number} restoreOnStartup Value of prefs.session.restore_on_startup.
   * @return {boolean}
   * @private
   */
  showIndicator_: function(ntpExtension, restoreOnStartup) {
    return !!ntpExtension && restoreOnStartup == this.prefValues_.OPEN_NEW_TAB;
  },

  /**
   * Determine whether to show the user defined startup pages.
   * @param {number} restoreOnStartup Enum value from prefValues_.
   * @return {boolean} Whether the open specific pages is selected.
   * @private
   */
  showStartupUrls_: function(restoreOnStartup) {
    return restoreOnStartup == this.prefValues_.OPEN_SPECIFIC;
  },
});
