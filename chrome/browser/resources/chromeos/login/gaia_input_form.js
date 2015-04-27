/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-input-form', (function() {
  var INPUT_EMAIL_PATTERN = "^[a-zA-Z0-9.!#$%&'*+=?^_`{|}~-]+(@[^\\s@]+)?$";

  return {

    inputValue: '',

    onButtonClicked: function() {
      this.fire('buttonClick');
    },

    onKeyDown: function(e) {
      this.setValid(true);
      this.setDomainVisibility();
      if (e.keyCode == 13 && !this.$.button.disabled)
        this.$.button.fire('tap');
    },

    onKeyUp: function(e) {
      this.setDomainVisibility();
    },

    setDomainVisibility: function() {
      this.$.emailDomain.hidden = !(this.inputValue.indexOf('@') === -1);
    },

    ready: function() {
      if (this.inputType == 'email') {
        this.$.inputForm.type = 'text';
        this.$.inputForm.pattern = INPUT_EMAIL_PATTERN;
        this.$.inputForm.addEventListener('keyup', this.onKeyUp.bind(this));
      } else {
        this.$.inputForm.type = this.inputType;
      }
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
  };
})());
