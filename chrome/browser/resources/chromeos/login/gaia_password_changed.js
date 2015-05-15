/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-password-changed', {
  invalidate: function() {
    this.$.oldPasswordInput.isInvalid = true;
  },

  reset: function() {
    this.$.animatedPages.selected = 0;
    this.clearPassword();
    this.$.oldPasswordInput.isInvalid = false;
    this.disabled = false;
    this.$.closeButton.hidden = false;
    this.$.oldPasswordCard.classList.remove('disabled');
  },

  ready: function() {
    this.$.oldPasswordInputForm.addEventListener('submit', function() {
      var inputPassword = this.$.oldPasswordInput.value;
      if (!inputPassword)
        this.invalidate();
      else {
        this.$.oldPasswordCard.classList.add('disabled');
        this.disabled = true;
        this.fire('passwordEnter', {password: inputPassword});
      }
    }.bind(this));
  },

  focus: function() {
    if (this.$.animatedPages.selected == 0)
      this.$.oldPasswordInput.focus();
  },

  set disabled(value) {
    this.$.oldPasswordInputForm.disabled = value;
  },

  onForgotPasswordClicked: function() {
    this.clearPassword();
    this.$.animatedPages.selected += 1;
  },

  onTryAgainClicked: function() {
    this.$.oldPasswordInput.isInvalid = false;
    this.$.animatedPages.selected -= 1;
  },

  onTransitionEnd: function() {
    this.focus();
  },

  clearPassword: function() {
    this.$.oldPasswordInput.value = '';
  },

  onProceedClicked: function() {
    this.disabled = true;
    this.$.closeButton.hidden = true;
    this.$.animatedPages.selected = 2;
    this.fire('proceedAnyway');
  },

  onClose: function() {
    this.disabled = true;
    this.fire('cancel');
  },
});
