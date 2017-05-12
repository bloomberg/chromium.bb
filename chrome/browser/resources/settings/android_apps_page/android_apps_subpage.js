// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-subpage' is the settings subpage for managing android apps.
 */

Polymer({
  is: 'settings-android-apps-subpage',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: Object,

    /** @private {!AndroidAppsInfo|undefined} */
    androidAppsInfo: {
      type: Object,
      observer: 'onAndroidAppsInfoUpdate_',
    },

    /** @private */
    dialogBody_: {
      type: String,
      value: function() {
        return this.i18nAdvanced(
            'androidAppsDisableDialogMessage',
            {substitutions: [], tags: ['br']});
      }
    }
  },

  /** @private {?settings.AndroidAppsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.AndroidAppsBrowserProxyImpl.getInstance();
  },

  /**
   * @private
   */
  onAndroidAppsInfoUpdate_: function() {
    if (!this.androidAppsInfo.playStoreEnabled)
      settings.navigateToPreviousRoute();
  },

  /**
   * @param {Event} event
   * @private
   */
  onManageAndroidAppsKeydown_: function(event) {
    if (event.key != 'Enter' && event.key != ' ')
      return;
    this.browserProxy_.showAndroidAppsSettings(true /** keyboardAction */);
    event.stopPropagation();
  },

  /** @private */
  onManageAndroidAppsTap_: function(event) {
    this.browserProxy_.showAndroidAppsSettings(false /** keyboardAction */);
  },

  /**
   * @return {boolean}
   * @private
   */
  allowRemove_: function() {
    return this.prefs.arc.enabled.enforcement !=
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * Shows a confirmation dialog when disabling android apps.
   * @param {Event} event
   * @private
   */
  onRemoveTap_: function(event) {
    this.$.confirmDisableDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onConfirmDisableDialogConfirm_: function() {
    this.setPrefValue('arc.enabled', false);
    this.$.confirmDisableDialog.close();
    settings.navigateToPreviousRoute();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onConfirmDisableDialogCancel_: function() {
    this.$.confirmDisableDialog.close();
  },

  /** @private */
  onConfirmDisableDialogClose_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#remove button')));
  },
});
