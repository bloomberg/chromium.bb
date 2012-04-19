// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var SettingsDialog = options.SettingsDialog;

  /*
   * SpellingConfirmOverlay class
   * Dialog to confirm that the user really wants to use the Spelling service
   * @extends {SettingsDialog}
   */
  function SpellingConfirmOverlay() {
    SettingsDialog.call(this,
                        'spellingConfirm',
                        templateData.spellingConfirmTitle,
                        'spelling-confirm-overlay',
                        $('spelling-confirm-ok'),
                        $('spelling-confirm-cancel'));
  };

  cr.addSingletonGetter(SpellingConfirmOverlay);

  SpellingConfirmOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /** @inheritDoc */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);
    },

    /** @inheritDoc */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);
      Preferences.setBooleanPref('spellcheck.use_spelling_service', true);
      Preferences.setBooleanPref('spellcheck.confirm_dialog_shown', true);
    },

    /** @inheritDoc */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      $('spelling-enabled-control').checked = false;
    },
  };

  // Export
  return {
    SpellingConfirmOverlay: SpellingConfirmOverlay
  };
});
