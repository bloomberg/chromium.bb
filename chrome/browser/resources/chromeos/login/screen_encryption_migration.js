// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('EncryptionMigrationScreen', 'encryption-migration',
    function() {
  return {
    EXTERNAL_API: [
    ],

    /**
     * Ignore any accelerators the user presses on this screen.
     */
    ignoreAccelerators: true,

    /** @override */
    decorate: function() {
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
  };
});
