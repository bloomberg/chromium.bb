// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-page' is the settings page for enabling android apps.
 */

Polymer({
  is: 'settings-android-apps-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!AndroidAppsInfo|undefined} */
    androidAppsInfo_: Object,
  },

  /** @private {?settings.AndroidAppsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.AndroidAppsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    cr.addWebUIListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    this.browserProxy_.requestAndroidAppsInfo();
  },
  /**
   * @param {AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_: function(info) {
    this.androidAppsInfo_ = info;
  },

  /** @return {string} */
  getSubtext_: function() {
    // TODO(stevenjb): Change the text when appReady to 'toggleOn' or whatever
    // UX finally decides on. Discussion in crbug.com/698463.
    return this.androidAppsInfo_.appReady ? this.i18n('androidAppsSubtext') :
                                            this.i18n('androidAppsSubtext');
  },

  /**
   * @param {Event} event
   * @private
   */
  onEnableTap_: function(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /** @private */
  onSubpageTap_: function() {
    if (!this.androidAppsInfo_.appReady)
      return;
    settings.navigateTo(settings.Route.ANDROID_APPS_DETAILS);
  },
});
