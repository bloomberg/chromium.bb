/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-input', (function() {
  var INPUT_EMAIL_PATTERN = "^[a-zA-Z0-9.!#$%&'*+=?^_`{|}~-]+(@[^\\s@]+)?$";

  return {
    required: false,

    ready: function() {
      this.typeChanged();
    },

    onKeyDown: function(e) {
      this.isInvalid = false;
    },

    setDomainVisibility: function() {
      this.$.domainLabel.hidden = (this.type !== 'email') || !this.domain ||
          (this.value.indexOf('@') !== -1);
    },

    onTap: function() {
      this.isInvalid = false;
    },

    focus: function() {
      this.$.input.focus();
    },

    checkValidity: function() {
      var isValid = this.$.input.validity.valid;
      this.isInvalid = !isValid;
      return isValid;
    },

    typeChanged: function() {
      if (this.type == 'email') {
        this.$.input.pattern = INPUT_EMAIL_PATTERN;
        this.$.input.type = 'text';
      } else {
        this.$.input.removeAttribute('pattern');
        this.$.input.type = this.type;
      }
      this.setDomainVisibility();
    },

    valueChanged: function() {
      this.setDomainVisibility();
    },

    domainChanged: function() {
      this.setDomainVisibility();
    }
  };
})());

