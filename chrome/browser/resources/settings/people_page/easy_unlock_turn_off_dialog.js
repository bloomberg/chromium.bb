// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog allowing the user to turn off the Easy Unlock feature.
 */

(function() {

/**
 * Possible UI statuses for the EasyUnlockTurnOffDialogElement.
 * See easy_unlock_settings_handler.cc.
 * @enum {string}
 */
var EasyUnlockTurnOffStatus = {
  UNKNOWN: 'unknown',
  OFFLINE: 'offline',
  IDLE: 'idle',
  PENDING: 'pending',
  SERVER_ERROR: 'server-error',
};

Polymer({
  is: 'easy-unlock-turn-off-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {!EasyUnlockTurnOffStatus} */
    status_: {
      type: String,
      value: EasyUnlockTurnOffStatus.UNKNOWN,
    },
  },

  /** @private {settings.EasyUnlockBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.EasyUnlockBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'easy-unlock-enabled-status',
        this.handleEasyUnlockEnabledStatusChanged_.bind(this));

    this.addWebUIListener(
        'easy-unlock-turn-off-flow-status',
        function(status) { this.status_ = status; }.bind(this));

    // Since the dialog text depends on the status, defer opening until we have
    // retrieved the turn off status to prevent UI flicker.
    this.getTurnOffStatus_().then(function(status) {
      this.status_ = status;
      this.$.dialog.showModal();
    }.bind(this));
  },

  /**
   * @return {!Promise<!EasyUnlockTurnOffStatus>}
   * @private
   */
  getTurnOffStatus_: function() {
    return navigator.onLine ?
        this.browserProxy_.getTurnOffFlowStatus() :
        Promise.resolve(EasyUnlockTurnOffStatus.OFFLINE);
  },

  /**
   * This dialog listens for Easy Unlock to become disabled. This signals
   * that the turnoff process has succeeded. Regardless of whether the turnoff
   * was initiated from this tab or another, this closes the dialog.
   * @param {boolean} easyUnlockEnabled
   * @private
   */
  handleEasyUnlockEnabledStatusChanged_: function(easyUnlockEnabled) {
    var dialog = /** @type {!CrDialogElement} */ this.$.dialog;
    if (!easyUnlockEnabled && dialog.open)
      this.onCancelTap_();
  },

  /** @private */
  onCancelTap_: function() {
    this.browserProxy_.cancelTurnOffFlow();
    this.$.dialog.close();
  },

  /** @private */
  onTurnOffTap_: function() {
    this.browserProxy_.startTurnOffFlow();
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {string}
   * @private
   */
  getTitleText_: function(status) {
    switch (status) {
      case EasyUnlockTurnOffStatus.OFFLINE:
        return this.i18n('easyUnlockTurnOffOfflineTitle');
      case EasyUnlockTurnOffStatus.UNKNOWN:
      case EasyUnlockTurnOffStatus.IDLE:
      case EasyUnlockTurnOffStatus.PENDING:
        return this.i18n('easyUnlockTurnOffTitle');
      case EasyUnlockTurnOffStatus.SERVER_ERROR:
        return this.i18n('easyUnlockTurnOffErrorTitle');
    }
    assertNotReached();
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {string}
   * @private
   */
  getDescriptionText_: function(status) {
    switch (status) {
      case EasyUnlockTurnOffStatus.OFFLINE:
        return this.i18n('easyUnlockTurnOffOfflineMessage');
      case EasyUnlockTurnOffStatus.UNKNOWN:
      case EasyUnlockTurnOffStatus.IDLE:
      case EasyUnlockTurnOffStatus.PENDING:
        return this.i18n('easyUnlockTurnOffDescription');
      case EasyUnlockTurnOffStatus.SERVER_ERROR:
        return this.i18n('easyUnlockTurnOffErrorMessage');
    }
    assertNotReached();
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {string}
   * @private
   */
  getTurnOffButtonText_: function(status) {
    switch (status) {
      case EasyUnlockTurnOffStatus.OFFLINE:
        return '';
      case EasyUnlockTurnOffStatus.UNKNOWN:
      case EasyUnlockTurnOffStatus.IDLE:
      case EasyUnlockTurnOffStatus.PENDING:
        return this.i18n('easyUnlockTurnOffButton');
      case EasyUnlockTurnOffStatus.SERVER_ERROR:
        return this.i18n('easyUnlockTurnOffRetryButton');
    }
    assertNotReached();
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {boolean}
   * @private
   */
  isButtonBarHidden_: function(status) {
    return status == EasyUnlockTurnOffStatus.OFFLINE;
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {boolean}
   * @private
   */
  isSpinnerActive_: function(status) {
    return status == EasyUnlockTurnOffStatus.PENDING;
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {boolean}
   * @private
   */
  isCancelButtonHidden_: function(status) {
    return status == EasyUnlockTurnOffStatus.SERVER_ERROR;
  },

  /**
   * @param {!EasyUnlockTurnOffStatus} status
   * @return {boolean}
   * @private
   */
  isTurnOffButtonEnabled_: function(status) {
    return status == EasyUnlockTurnOffStatus.IDLE ||
        status == EasyUnlockTurnOffStatus.SERVER_ERROR;
  },
});

})();
