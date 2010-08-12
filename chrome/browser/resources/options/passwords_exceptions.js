// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // PasswordsExceptions class:

  /**
   * Encapsulated handling of password and exceptions page.
   * @constructor
   */
  function PasswordsExceptions() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'passwordsExceptions',
                     templateData.passwordsExceptionsTitle,
                     'passwordsExceptionsPage');
  }

  cr.addSingletonGetter(PasswordsExceptions);

  PasswordsExceptions.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      // TODO(sargrass): Passwords filter page --------------------------

      // TODO(sargrass): Exceptions filter page -------------------------

    },

    setAutofillableLogins_: function(autofillableLogins) {
      var ul = $('autofillableLoginsList');
      for (var i = 0; i < autofillableLogins.length; i++) {
        var entry = autofillableLogins[i];
        var li = document.createElement('li');
        li.textContent = "URL=" + entry[0] + " Username=" + entry[1];
        ul.appendChild(li);
      }
    },
  };

  PasswordsExceptions.setAutofillableLogins = function(autofillableLogins) {
    PasswordsExceptions.getInstance().setAutofillableLogins_(
        autofillableLogins);
  };

  // Export
  return {
    PasswordsExceptions: PasswordsExceptions
  };

});

