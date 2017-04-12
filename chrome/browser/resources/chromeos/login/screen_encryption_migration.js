// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('EncryptionMigrationScreen', 'encryption-migration',
    function() {
  return {
    EXTERNAL_API: [
      'setUIState',
      'setMigrationProgress',
      'setIsResuming',
    ],

    /**
     * Ignore any accelerators the user presses on this screen.
     */
    ignoreAccelerators: true,

    /** @override */
    decorate: function() {
      var encryptionMigration = $('encryption-migration-element');
      encryptionMigration.addEventListener('upgrade', function() {
        chrome.send('startMigration');
      });
      encryptionMigration.addEventListener('skip', function() {
        chrome.send('skipMigration');
      });
      encryptionMigration.addEventListener('restart', function() {
        chrome.send('requestRestart');
      });
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     */
    onBeforeShow: function() {
      $('progress-dots').hidden = true;
      var headerBar = $('login-header-bar');
      headerBar.allowCancel = false;
      headerBar.showGuestButton = false;
      headerBar.showCreateSupervisedButton = false;
      headerBar.signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Updates the migration screen by specifying a state which corresponds to
     * a sub step in the migration process.
     * @param {number} state The UI state to identify a sub step in migration.
     */
    setUIState: function(state) {
      $('encryption-migration-element').uiState = state;
    },

    /**
     * Updates the migration progress.
     * @param {number} progress The progress of migration in range [0, 1].
     */
    setMigrationProgress: function(progress) {
      $('encryption-migration-element').progress = progress;
    },

    /**
     * Updates the migration screen based on whether the migration process is
     * resuming the previous one.
     * @param {boolean} isResuming
     */
    setIsResuming: function(isResuming) {
      $('encryption-migration-element').isResuming = isResuming;
    },
  };
});
