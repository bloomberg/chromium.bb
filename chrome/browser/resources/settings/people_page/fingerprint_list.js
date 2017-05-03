// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * The duration in ms of a background flash when a user touches the fingerprint
 * sensor on this page.
 * @const {number}
 */
var FLASH_DURATION_MS = 500;

Polymer({
  is: 'settings-fingerprint-list',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * The list of fingerprint objects.
     * @private {!Array<string>}
     */
    fingerprints_: {
      type: Array,
      value: function() {
        return [];
      }
    }
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'on-fingerprint-attempt-received', this.onAttemptReceived_.bind(this));
    this.addWebUIListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.browserProxy_ = settings.FingerprintBrowserProxyImpl.getInstance();
    this.browserProxy_.startAuthentication();
    this.updateFingerprintsList_();
  },

  /** @override */
  detached: function() {
    this.browserProxy_.endCurrentAuthentication();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute != settings.Route.FINGERPRINT) {
      if (this.browserProxy_)
        this.browserProxy_.endCurrentAuthentication();
    } else if (oldRoute == settings.Route.LOCK_SCREEN) {
      // Start fingerprint authentication when going from LOCK_SCREEN to
      // FINGERPRINT page.
      this.browserProxy_.startAuthentication();
    }
  },

  /**
   * Sends a ripple when the user taps the sensor with a registered fingerprint.
   * @param {!settings.FingerprintAttempt} fingerprintAttempt
   * @private
   */
  onAttemptReceived_: function(fingerprintAttempt) {
    /** @type {NodeList<!HTMLElement>} */ var listItems =
        this.$.fingerprintsList.querySelectorAll('.list-item');
    /** @type {Array<number>} */ var filteredIndexes =
        fingerprintAttempt.indexes.filter(function(index) {
          return index >= 0 && index < listItems.length;
        });

    // Flash the background and produce a ripple for each list item that
    // corresponds to the attempted finger.
    filteredIndexes.forEach(function(index) {
      var listItem = listItems[index];
      var ripple = listItem.querySelector('paper-ripple');

      // Activate the ripple.
      if (ripple)
        ripple.simulatedRipple();

      // Flash the background.
      listItem.animate({
        backgroundColor: ['var(--google-grey-300)', 'white'],
      }, FLASH_DURATION_MS);
    });
  },

  /** @private */
  updateFingerprintsList_: function() {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  },

  /**
   * @param {!settings.FingerprintInfo} fingerprintInfo
   * @private
   */
  onFingerprintsChanged_: function(fingerprintInfo) {
    // Update iron-list.
    this.fingerprints_ = fingerprintInfo.fingerprintsList.slice();
    this.$$('.action-button').disabled = fingerprintInfo.isMaxed;
  },

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onFingerprintDeleteTapped_: function(e) {
    this.browserProxy_.removeEnrollment(e.model.index).then(
        function(success) {
          if (success)
            this.updateFingerprintsList_();
        }.bind(this));
  },

  /**
   * @param {!{model: !{index: !number, item: !string}}} e
   * @private
   */
  onFingerprintLabelChanged_: function(e) {
    this.browserProxy_.changeEnrollmentLabel(e.model.index, e.model.item).then(
        function(success) {
          if (success)
            this.updateFingerprintsList_();
        }.bind(this));
  },

  /**
   * Opens the setup fingerprint dialog.
   * @private
   */
  openAddFingerprintDialog_: function() {
    this.$.setupFingerprint.open();
  },

  /** @private */
  onSetupFingerprintDialogClose_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#addFingerprint')));
    this.browserProxy_.startAuthentication();
  },

  /**
   * Close the setup fingerprint dialog when the screen is unlocked.
   * @param {boolean} screenIsLocked
   * @private
   */
  onScreenLocked_: function(screenIsLocked) {
    if (!screenIsLocked &&
        settings.getCurrentRoute() == settings.Route.FINGERPRINT) {
      this.$.setupFingerprint.close();
    }
  },
});
})();
