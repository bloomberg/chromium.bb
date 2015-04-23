/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-input-form', {
  inputValue: '',

  onButtonClicked: function() {
    this.fire('buttonClick');
  },

  onKeyDown: function(e) {
    this.setValid(true);
    if (e.keyCode == 13 && !this.$.button.disabled)
      this.$.button.fire('tap');
  },

  onTap: function() {
    this.setValid(true);
  },

  focus: function() {
    this.$.inputForm.focus();
  },

  checkValidity: function() {
    var input = this.$.inputForm;
    var isValid = input.validity.valid;
    this.setValid(isValid);
    return isValid;
  },

  setValid: function(isValid) {
    this.$.paperInputDecorator.isInvalid = !isValid;
  }
});
