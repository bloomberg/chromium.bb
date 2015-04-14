/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('offline-gaia', (function() {
  return {
    onTransitionEnd: function() {
      this.focus();
    },

    focus: function() {
      if (this.$.animatedPages.selected == 0)
        this.$.emailInput.focus();
      else
        this.$.passwordInput.focus();
    },

    onForgotPasswordClicked: function() {
      this.$.forgotPasswordDlg.toggle();
    },

    ready: function() {
      var emailInput = this.$.emailInput;
      var passwordInput = this.$.passwordInput;
      var passwordHeader = this.$.passwordHeader;
      var animatedPages = this.$.animatedPages;
      emailInput.addEventListener('buttonClick', function() {
        if (emailInput.checkValidity()) {
          passwordHeader.email = emailInput.inputValue;
          passwordInput.setValid(true);
          animatedPages.selected += 1;
        } else {
          emailInput.focus();
        }
      });

      passwordHeader.addEventListener('backClick', function() {
        passwordInput.inputValue = '';
        animatedPages.selected -= 1;
      });

      passwordInput.addEventListener('buttonClick', function() {
        if (this.$.passwordInput.checkValidity()) {
          var msg = {
            'useOffline': true,
            'email': this.$.emailInput.inputValue,
            'password': this.$.passwordInput.inputValue
          };
          this.$.passwordInput.inputValue = '';
          this.fire('authCompleted', msg);
        }
      }.bind(this));

    },

    setEmail: function(email) {
      if (email) {
        this.$.emailInput.inputValue = email;
        this.$.passwordHeader.email = email;
        this.$.animatedPages.selected = 1;
        this.$.passwordInput.setValid(false);
      } else {
        this.$.emailInput.inputValue = '';
        this.$.animatedPages.selected = 0;
      }
    }
  };
})());
