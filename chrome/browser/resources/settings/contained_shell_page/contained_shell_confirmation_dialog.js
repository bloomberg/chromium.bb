// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-contained-shell-confirmation-dialog' is a dialog shown to confirm
 * if a Contained Shell change is really wanted. Since enabling/disabling the
 * shell requires a sign out, we need to provide this dialog to avoid surprising
 * users.
 */
Polymer({
  is: 'settings-contained-shell-confirmation-dialog',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onCancelTap_: function(event) {
    this.$.dialog.close();
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onConfirmTap_: function(event) {
    const prefPath = 'ash.contained_shell.enabled';
    this.setPrefValue(prefPath, !this.getPref(prefPath).value);
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    event.stopPropagation();
  },

  /**
   * @private
   * @return {string}
   */
  getTitleText_: function(containedShellEnabled) {
    return containedShellEnabled ?
        this.i18n('containedShellEnabledDialogTitle') :
        this.i18n('containedShellDisabledDialogTitle');
  },

  /**
   * @private
   * @return {string}
   */
  getBodyText_: function(containedShellEnabled) {
    return containedShellEnabled ?
        this.i18n('containedShellEnabledDialogBody') :
        this.i18n('containedShellDisabledDialogBody');
  },

  /**
   * @private
   * @return {string}
   */
  getConfirmationText_: function(containedShellEnabled) {
    return containedShellEnabled ? this.i18n('containedShellTurnOff') :
                                   this.i18n('containedShellTurnOn');
  },
});
