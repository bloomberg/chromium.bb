// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-contained-shell-page' is the settings page for enabling the
 * Contained Shell.
 */
Polymer({
  is: 'settings-contained-shell-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showConfirmationDialog_: Boolean,
  },

  /**
   * @private
   * @param {!Event} event
   */
  onToggleButtonPressed_: function(event) {
    this.showConfirmationDialog_ = true;
    event.stopPropagation();
  },

  /**
   * @private
   * @param {!Event} event
   */
  onConfirmationDialogClose_: function(event) {
    this.showConfirmationDialog_ = false;
    event.stopPropagation();
  },

  /**
   * @private
   * @param {boolean} containedShellEnabled
   * @return {string}
   */
  getSubtextLabel_: function(containedShellEnabled) {
    return containedShellEnabled
        ? this.i18n('containedShellPageSubtextDisable')
        : this.i18n('containedShellPageSubtextEnable');
  },

  /**
   * @private
   * @param {boolean} containedShellEnabled
   * @return {string}
   */
  getButtonLabel_: function(containedShellEnabled) {
    return containedShellEnabled
        ? this.i18n('containedShellTurnOff')
        : this.i18n('containedShellTurnOn');
  }
});
