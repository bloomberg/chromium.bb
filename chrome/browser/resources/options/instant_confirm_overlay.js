// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var SettingsDialog = options.SettingsDialog;

  /*
   * InstantConfirmOverlay class
   * Dialog to confirm that the user really wants to enable Chrome Instant.
   * @extends {SettingsDialog}
   */
  function InstantConfirmOverlay() {
    SettingsDialog.call(this,
                        'instantConfirm',
                        loadTimeData.getString('instantConfirmTitle'),
                        'instantConfirmOverlay',
                        $('instantConfirmOk'),
                        $('instantConfirmCancel'));
  };

  cr.addSingletonGetter(InstantConfirmOverlay);

  InstantConfirmOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /** @inheritDoc */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);
    },

    /** @inheritDoc */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);
      Preferences.setBooleanPref('instant.confirm_dialog_shown', true, true);
      Preferences.setBooleanPref('instant.enabled', true, true);
    },

    /** @inheritDoc */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      $('instant-enabled-control').checked = false;
    },
  };

  // Export
  return {
    InstantConfirmOverlay: InstantConfirmOverlay
  };
});
