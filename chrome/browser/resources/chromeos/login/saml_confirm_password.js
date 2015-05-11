/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('saml-confirm-password', {
  onClose: function() {
    this.disabled = true;
    this.$.cancelConfirmDlg.toggle();
  },

  onConfirmCancel: function() {
    this.fire('cancel');
  },

  reset: function() {
    this.$.cancelConfirmDlg.close();
    this.disabled = false;
    this.$.closeButton.hidden = false;
    this.$.animatedPages.selected = 0;
    this.$.passwordInput.value = '';
  },

  invalidate: function() {
    this.$.passwordInput.isInvalid = true;
  },

  focus: function() {
    if (this.$.animatedPages.selected == 0)
      this.$.passwordInput.focus();
  },

  onPasswordSubmitted: function() {
    var inputPassword = this.$.passwordInput.value;
    this.$.passwordInput.value = '';
    if (!inputPassword) {
      this.invalidate();
    } else {
      this.$.animatedPages.selected += 1;
      this.$.closeButton.hidden = true;
      this.fire('passwordEnter', {password: inputPassword});
    }
  },

  set disabled(value) {
    this.$.confirmPasswordCard.classList.toggle('full-disabled', value);
    this.$.inputForm.disabled = value;
    this.$.closeButton.disabled = value;
  },

  ready: function() {
    this.$.cancelConfirmDlg.addEventListener('core-overlay-close-completed',
        function() {
          this.disabled = false;
        }.bind(this));
  }
});
