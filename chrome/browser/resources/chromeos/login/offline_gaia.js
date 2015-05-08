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
        this.$.passwordInput.isInvalid = true;
      } else {
        this.$.emailInput.value = '';
        this.switchToEmailCard();
      }
    },

    onBack: function() {
      this.switchToEmailCard();
    },

    switchToEmailCard() {
      this.$.passwordInput.value = '';
      this.$.passwordInput.isInvalid = false;
      this.$.emailInput.isInvalid = false;
      this.$.backButton.hidden = true;
      this.$.animatedPages.selected = 'emailSection';
    },

    switchToPasswordCard(email) {
      this.$.emailInput.value = email;
      if (email.indexOf('@') === -1) {
        if (this.emailDomain)
          email = email + this.emailDomain;
        else
          email = email + DEFAULT_EMAIL_DOMAIN;
      }
      this.$.passwordHeader.email = email;
      this.$.backButton.hidden = false;
      this.$.animatedPages.selected = 'passwordSection';
    },

    onEmailSubmitted: function() {
      if (this.$.emailInput.checkValidity())
        this.switchToPasswordCard(this.$.emailInput.value);
      else
        this.$.emailInput.focus();
    },

    onPasswordSubmitted: function() {
      if (!this.$.passwordInput.checkValidity())
        return;
      var msg = {
        'useOffline': true,
        'email': this.$.passwordHeader.email,
        'password': this.$.passwordInput.value
      };
      this.$.passwordInput.value = '';
      this.fire('authCompleted', msg);
    }
  };
})());
