// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */
Polymer({
  is: 'active-directory-password-change',

  properties: {
    /**
     * The user principal name.
     */
    username: String,
  },

  /** @public */
  reset: function() {
    this.$.animatedPages.selected = 0;
    this.$.oldPassword.value = '';
    this.$.newPassword1.value = '';
    this.$.newPassword2.value = '';
    this.updateNavigation_();
  },

  /** @private */
  computeWelcomeMessage_: function(username) {
    return loadTimeData.getStringF('adPasswordChangeMessage', username);
  },

  /** @private */
  onSubmit_: function() {
    if (!this.$.oldPassword.checkValidity() ||
        !this.$.newPassword1.checkValidity()) {
      return;
    }
    if (this.$.newPassword1.value != this.$.newPassword2.value) {
      this.$.newPassword2.isInvalid = true;
      return;
    }
    this.$.animatedPages.selected++;
    this.updateNavigation_();
    var msg = {
      'username': this.username,
      'oldPassword': this.$.oldPassword.value,
      'newPassword': this.$.newPassword1.value,
    };
    this.$.oldPassword.value = '';
    this.$.newPassword1.value = '';
    this.$.newPassword2.value = '';
    this.fire('authCompleted', msg);
  },

  /** @private */
  onClose_: function() {
    if (!this.$.navigation.closeVisible)
      return;
    this.fire('cancel');
  },

  /** @private */
  updateNavigation_: function() {
    this.$.navigation.closeVisible = (this.$.animatedPages.selected == 0);
  },
});
