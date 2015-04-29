/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('offline-gaia', (function() {
  var DEFAULT_EMAIL_DOMAIN = '@gmail.com';

  return {
    onTransitionEnd: function() {
      this.focus();
    },

    focus: function() {
      if (this.$.animatedPages.selected == 'emailSection')
        this.$.emailInput.focus();
      else
        this.$.passwordInput.focus();
    },

    onForgotPasswordClicked: function() {
      this.$.forgotPasswordDlg.toggle();
    },

    onForgotPasswordKeyDown: function(e) {
      if (e.keyCode == 13 || e.keyCode == 32)
        return this.onForgotPasswordClicked();
    },

    onKeyDownOnDialog: function(e) {
      if (e.keyCode == 27) {
        // Esc
        this.$.forgotPasswordDlg.close();
        e.preventDefault();
      }
    },

    ready: function() {
      var emailInput = this.$.emailInput;
      var passwordInput = this.$.passwordInput;
      emailInput.addEventListener('buttonClick', function() {
        if (emailInput.checkValidity()) {
          this.switchToPasswordCard(emailInput.inputValue);
        } else {
          emailInput.focus();
        }
      }.bind(this));

      passwordInput.addEventListener('buttonClick', function() {
        if (this.$.passwordInput.checkValidity()) {
          var msg = {
            'useOffline': true,
            'email': this.$.passwordHeader.email,
            'password': this.$.passwordInput.inputValue
          };
          this.$.passwordInput.inputValue = '';
          this.fire('authCompleted', msg);
        }
      }.bind(this));

    },

    setEmail: function(email) {
      // Reorder elements for proper animation for rtl languages.
      if (document.querySelector('html[dir=rtl]')) {
        this.$.emailSection.parentNode.insertBefore(this.$.passwordSection,
                                                    this.$.emailSection);
      }
      if (email) {
        if (this.emailDomain)
          email = email.replace(this.emailDomain, '');
        this.switchToPasswordCard(email);
        this.$.passwordInput.setValid(false);
      } else {
        this.$.emailInput.inputValue = '';
        this.switchToEmailCard();
      }
    },

    onBack: function() {
      this.switchToEmailCard();
    },

    switchToEmailCard() {
      this.$.passwordInput.inputValue = '';
      this.$.passwordInput.setValid(true);
      this.$.emailInput.setValid(true);
      this.$.backButton.hidden = true;
      this.$.animatedPages.selected = 'emailSection';
    },

    switchToPasswordCard(email) {
      this.$.emailInput.inputValue = email;
      if (email.indexOf('@') === -1) {
        if (this.emailDomain)
          email = email + this.emailDomain;
        else
          email = email + DEFAULT_EMAIL_DOMAIN;
      }
      this.$.passwordHeader.email = email;
      this.$.backButton.hidden = false;
      this.$.animatedPages.selected = 'passwordSection';
    }
  };
})());
