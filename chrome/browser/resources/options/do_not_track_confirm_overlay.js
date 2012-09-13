// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var SettingsDialog = options.SettingsDialog;

  /**
   * DoNotTrackConfirmOverlay class
   * Dialog to confirm that the user really wants to enable Do Not Track.
   * @extends {SettingsDialog}
   */
  function DoNotTrackConfirmOverlay() {
    SettingsDialog.call(this,
                        'doNotTrackConfirm',
                        loadTimeData.getString('doNotTrackConfirmTitle'),
                        'do-not-track-confirm-overlay',
                        $('do-not-track-confirm-ok'),
                        $('do-not-track-confirm-cancel'));
  };

  cr.addSingletonGetter(DoNotTrackConfirmOverlay);

  DoNotTrackConfirmOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /** @inheritDoc */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);
      Preferences.setBooleanPref('enable_do_not_track', true, true);
    },

    /** @inheritDoc */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      $('do-not-track-enabled').checked = false;
    },
  };

  // Export
  return {
    DoNotTrackConfirmOverlay: DoNotTrackConfirmOverlay
  };
});
