// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'saml-confirm-password',

  properties: {
    email: String,

    disabled: {
      type: Boolean,
      value: false,
      observer: 'disabledChanged_'
    }
  },

  ready: function() {
    /**
     * Workaround for
     * https://github.com/PolymerElements/neon-animation/issues/32
     * TODO(dzhioev): Remove when fixed in Polymer.
     */
    var pages = this.$.animatedPages;
    delete pages._squelchNextFinishEvent;
    Object.defineProperty(pages, '_squelchNextFinishEvent',
        { get: function() { return false; } });
  },

  reset: function() {
    this.$.cancelConfirmDlg.close();
    this.disabled = false;
    this.$.navigation.closeVisible = true;
    if (this.$.animatedPages.selected != 0)
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

  onClose_: function() {
    this.disabled = true;
    this.$.cancelConfirmDlg.fitInto = this;
    this.$.cancelConfirmDlg.open();
  },

  onConfirmCancel_: function() {
    this.fire('cancel');
  },

  onPasswordSubmitted_: function() {
    if (!this.$.passwordInput.checkValidity())
      return;
    this.$.animatedPages.selected = 1;
    this.$.navigation.closeVisible = false;
    this.fire('passwordEnter', {password: this.$.passwordInput.value});
  },

  onDialogOverlayClosed_: function() {
    this.disabled = false;
  },

  disabledChanged_: function(disabled) {
    this.$.confirmPasswordCard.classList.toggle('full-disabled', disabled);
  },

  onAnimationFinish_: function() {
    if (this.$.animatedPages.selected == 1)
      this.$.passwordInput.value = '';
  }
});
