// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-password-screen',

  properties: {
    invalid: Boolean,

    strings: Object,
  },

  get active() {
    return this.$.dialog.open;
  },

  show: function() {
    this.$.dialog.showModal();
  },

  close: function() {
    if (this.active)
      this.$.dialog.close();
  },

  deny: function() {
    this.$.password.disabled = false;
    this.$.submit.disabled = false;
    this.invalid = true;
    this.$.password.focus();
    this.$.password.inputElement.select();
  },

  submit: function() {
    if (this.$.password.value.length == 0)
      return;
    this.$.password.disabled = true;
    this.$.submit.disabled = true;
    this.fire('password-submitted', {password: this.$.password.value});
  },
});
