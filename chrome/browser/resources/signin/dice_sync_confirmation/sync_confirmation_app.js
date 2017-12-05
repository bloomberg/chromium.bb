/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

Polymer({
  is: 'sync-confirmation-app',

  listeners: {
    // This is necessary since the settingsLink element is inserted by i18nRaw.
    'settingsLink.tap': 'onGoToSettings_'
  },

  /** @private {?sync.confirmation.SyncConfirmationBrowserProxy} */
  syncConfirmationBrowserProxy_: null,

  /** @private {?function(Event)} */
  boundKeyDownHandler_: null,

  /** @override */
  attached: function() {
    this.syncConfirmationBrowserProxy_ =
        sync.confirmation.SyncConfirmationBrowserProxyImpl.getInstance();
    this.boundKeyDownHandler_ = this.onKeyDown_.bind(this);
    // This needs to be bound to document instead of "this" because the dialog
    // window opens initially, the focus level is only on document, so the key
    // event is not captured by "this".
    document.addEventListener('keydown', this.boundKeyDownHandler_);
  },

  /** @override */
  detached: function() {
    document.removeEventListener('keydown', this.boundKeyDownHandler_);
  },

  /** @private */
  onConfirm_: function() {
    this.syncConfirmationBrowserProxy_.confirm();
  },

  /** @private */
  onUndo_: function() {
    this.syncConfirmationBrowserProxy_.undo();
  },

  /** @private */
  onGoToSettings_: function() {
    this.syncConfirmationBrowserProxy_.goToSettings();
  },

  /** @private */
  onKeyDown_: function(e) {
    if (e.key == 'Enter' && !/^(A|PAPER-BUTTON)$/.test(e.path[0].tagName)) {
      this.onConfirm_();
      e.preventDefault();
    }
  },
});
