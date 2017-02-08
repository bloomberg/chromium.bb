// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Active Directory password change screen implementation.
 */
login.createScreen('ActiveDirectoryPasswordChangeScreen', 'ad-password-change',
    function() {
  return {
    EXTERNAL_API: [
      'show'
    ],

    adPasswordChanged_: null,

    /** @override */
    decorate: function() {
      this.adPasswordChanged_ = $('active-directory-password-change');
      this.adPasswordChanged_.addEventListener('cancel',
                                               this.cancel.bind(this));

      this.adPasswordChanged_.addEventListener('authCompleted',
          function(e) {
            chrome.send('completeAdPasswordChange',
                [e.detail.username,
                 e.detail.oldPassword,
                 e.detail.newPassword]);
          });
    },

    /**
     * Cancels password changing and drops the user back to the login screen.
     */
    cancel: function() {
      Oobe.showUserPods();
    },

    /**
     * Shows password changed screen.
     * @param {string} username Name of user that should change the password.
     */
    show: function(username) {
      // Active Directory password change screen is similar to Active Directory
      // login screen. So we restore bottom bar controls.
      Oobe.getInstance().headerHidden = false;
      Oobe.showScreen({id: SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE});
      this.adPasswordChanged_.reset();
      this.adPasswordChanged_.username = username;
    }
  };
});
