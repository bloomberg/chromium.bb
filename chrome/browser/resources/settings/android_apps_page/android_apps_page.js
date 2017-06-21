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

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        map.set(
            settings.Route.ANDROID_APPS_DETAILS.path,
            '#android-apps .subpage-arrow');
        return map;
      },
    },
  },

  /** @private {?settings.AndroidAppsBrowserProxy} */
  browserProxy_: null,

  /** @private {?WebUIListener} */
  listener_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.AndroidAppsBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.listener_ = cr.addWebUIListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    this.browserProxy_.requestAndroidAppsInfo();
  },

  /** @override */
  detached: function() {
    cr.removeWebUIListener(this.listener_);
  },

  /**
   * @param {AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_: function(info) {
    this.androidAppsInfo_ = info;
  },

  /**
   * @param {Event} event
   * @private
   */
  onEnableTap_: function(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /** @return {boolean} */
  isEnforced_: function(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onSubpageTap_: function() {
    if (this.androidAppsInfo_.playStoreEnabled)
      settings.navigateTo(settings.Route.ANDROID_APPS_DETAILS);
  },
});
