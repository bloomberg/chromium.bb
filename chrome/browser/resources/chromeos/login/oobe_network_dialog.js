// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network selection OOBE dialog.
 */

Polymer({
  is: 'oobe-network-dialog',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Whether device is connected to the network.
     * @private
     */
    isConnected_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready: function() {
    this.updateLocalizedContent();
  },

  /** Shows the dialog. */
  show: function() {
    this.$.networkDialog.show();
  },

  /** Updates localized elements of the UI. */
  updateLocalizedContent: function() {
    this.$.networkSelectLogin.setCrOncStrings();
    this.i18nUpdateLocale();
  },

  /**
   * Called after dialog is shown. Refreshes the list of the networks.
   * @private
   */
  onShown_: function() {
    this.async(function() {
      this.$.networkSelectLogin.refresh();
      this.$.networkSelectLogin.focus();
    }.bind(this));
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_: function() {
    this.fire('next-button-clicked');
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_: function() {
    this.fire('back-button-clicked');
  },

});
