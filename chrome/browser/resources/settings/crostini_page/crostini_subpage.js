// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

Polymer({
  is: 'settings-crostini-subpage',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showRemoveDialog_: {
      type: Boolean,
      value: false,
    },
  },

  observers: ['onCrostiniEnabledChanged_(prefs.crostini.enabled.value)'],

  /** @private */
  onCrostiniEnabledChanged_: function(enabled) {
    if (!enabled &&
        settings.getCurrentRoute() == settings.routes.CROSTINI_DETAILS) {
      settings.navigateToPreviousRoute();
    }
  },

  /**
   * Shows a confirmation dialog when removing crostini.
   * @param {!Event} event
   * @private
   */
  onRemoveTap_: function(event) {
    this.showRemoveDialog_ = true;
    this.async(() => this.$$('#removeDialog').showModal());
  },

  /**
   * Handles the remove confirmation dialog 'Confirm' button.
   * @private
   */
  onRemoveDialogAccept_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
    this.$$('#removeDialog').close();
  },

  /**
   * Handles the remove confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onRemoveDialogCancel_: function() {
    this.$$('#removeDialog').close();
  },

  /**
   * Handles the remove confirmation dialog close event.
   * @private
   */
  onRemoveDialogClose_: function() {
    this.showRemoveDialog_ = false;
  },
});
