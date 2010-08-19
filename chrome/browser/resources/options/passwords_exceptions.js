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
                     templateData.savedPasswordsExceptionsTitle,
                     'passwordsExceptionsPage');
  }

  cr.addSingletonGetter(PasswordsExceptions);

  PasswordsExceptions.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      options.passwordsExceptions.ListArea.decorate($('passwordsArea'));

      // TODO(sargrass): Passwords filter page --------------------------

      // TODO(sargrass): Exceptions filter page -------------------------

    },

    setAutofillableLogins_: function(entries) {
      autofillableLoginsList.clear();
      for (var i = 0; i < entries.length; i++) {
        autofillableLoginsList.addEntry(entries[i]);
      }
    },
  };

  PasswordsExceptions.load = function() {
    chrome.send('loadSavedPasswords');
  };

  PasswordsExceptions.removeAutofillable = function(index) {
    chrome.send('removeAutofillable', [String(index)]);
  };

  PasswordsExceptions.showSelectedPassword = function(index) {
    chrome.send('showSelectedPassword', [String(index)]);
  };

  PasswordsExceptions.setAutofillableLogins = function(entries) {
    PasswordsExceptions.getInstance().setAutofillableLogins_(entries);
  };

  PasswordsExceptions.selectedPasswordCallback = function(password) {
    passwordsArea.displayReturnedPassword(password);
  };

  // Export
  return {
    PasswordsExceptions: PasswordsExceptions
  };

});

