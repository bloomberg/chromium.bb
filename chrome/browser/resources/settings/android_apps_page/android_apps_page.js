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
    prefs: Object,

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

  /**
   * @param {Event} event
   * @private
   */
  onManageAndroidAppsKeydown_: function(event) {
    if (event.key != 'Enter' && event.key != ' ')
      return;
    this.browserProxy_.showAndroidAppsSettings(true  /** keyboardAction */);
    event.stopPropagation();
  },

  /** @private */
  onManageAndroidAppsTap_: function(event) {
    this.browserProxy_.showAndroidAppsSettings(false /** keyboardAction */);
  },

  /**
   * @return {string}
   * @private
   */
  getDialogBody_: function() {
    return this.i18nAdvanced(
        'androidAppsDisableDialogMessage', {substitutions: [], tags: ['br']});
  },

  /**
   * Handles the change event for the arc.enabled checkbox. Shows a
   * confirmation dialog when disabling the preference.
   * @param {Event} event
   * @private
   */
  onArcEnabledChange_: function(event) {
    if (event.target.checked) {
      /** @type {!SettingsCheckboxElement} */ (event.target).sendPrefChange();
      return;
    }
    this.$.confirmDisableDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onConfirmDisableDialogConfirm_: function() {
    /** @type {!SettingsCheckboxElement} */ (this.$.enabled).sendPrefChange();
    this.$.confirmDisableDialog.close();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onConfirmDisableDialogCancel_: function() {
    /** @type {!SettingsCheckboxElement} */ (this.$.enabled).resetToPrefValue();
    this.$.confirmDisableDialog.close();
  },

  /**
   * @param {!Event} e
   * @private
   */
  stopPropagation_: function(e) {
    e.stopPropagation();
  },
});
