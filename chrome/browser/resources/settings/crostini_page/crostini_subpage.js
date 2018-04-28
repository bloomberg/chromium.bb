// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

Polymer({
  is: 'settings-crostini-subpage',

  behaviors: [I18nBehavior, PrefsBehavior],

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

    /** @private */
    removingInProgress_: Boolean,

    /** @private */
    cancelButtonText_: {
      type: String,
      computed: 'computeCancelButtonText_(removingInProgress_)',
    },
  },

  observers: ['onCrostiniEnabledChanged_(prefs.crostini.enabled.value)'],

  /** @private */
  computeCancelButtonText_() {
    return this.i18n(this.removingInProgress_ ? 'close' : 'cancel');
  },

  /** @private */
  onCrostiniEnabledChanged_: function(enabled) {
    if (this.$$('#removeDialog'))
      this.$$('#removeDialog').close();
    this.removingInProgress_ = false;
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
    this.removingInProgress_ = true;
    // Sub-page will be closed in onCrostiniEnabledChanged_ call.
  },

  // TODO(rjwright): Make this actually cancel the uninstall.
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
