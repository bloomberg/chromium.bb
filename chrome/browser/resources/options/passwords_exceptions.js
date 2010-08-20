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

      options.passwordsExceptions.PasswordsListArea.decorate($('passwordsArea'));
      options.passwordsExceptions.PasswordExceptionsListArea.decorate(
          $('passwordExceptionsArea'));

      $('password-exceptions-nav-tab').onclick = function() {
        OptionsPage.showTab($('password-exceptions-nav-tab'));
        passwordExceptionsList.redraw();
      }
    },

    setSavedPasswordsList_: function(entries) {
      savedPasswordsList.clear();
      for (var i = 0; i < entries.length; i++) {
        savedPasswordsList.addEntry(entries[i]);
      }
    },

    setPasswordExceptionsList_: function(entries) {
      passwordExceptionsList.clear();
      for (var i = 0; i < entries.length; i++) {
        passwordExceptionsList.addEntry(entries[i]);
      }
    },

  };

  PasswordsExceptions.load = function() {
    chrome.send('loadLists');
  };

  /**
   * Call to remove a saved password.
   * @param rowIndex indicating the row to remove.
   */
  PasswordsExceptions.removeSavedPassword = function(rowIndex) {
      chrome.send('removeSavedPassword', [String(rowIndex)]);
  };

  /**
   * Call to remove a password exception.
   * @param rowIndex indicating the row to remove.
   */
  PasswordsExceptions.removePasswordException = function(rowIndex) {
      chrome.send('removePasswordException', [String(rowIndex)]);
  };


  /**
   * Call to remove all saved passwords.
   * @param tab contentType of the tab currently on.
   */
  PasswordsExceptions.removeAllPasswords = function() {
    chrome.send('removeAllSavedPasswords');
  };

  /**
   * Call to remove all saved passwords.
   * @param tab contentType of the tab currently on.
   */
  PasswordsExceptions.removeAllPasswordExceptions = function() {
    chrome.send('removeAllPasswordExceptions');
  };

  PasswordsExceptions.showSelectedPassword = function(index) {
    chrome.send('showSelectedPassword', [String(index)]);
  };

  PasswordsExceptions.setSavedPasswordsList = function(entries) {
    PasswordsExceptions.getInstance().setSavedPasswordsList_(entries);
  };

  PasswordsExceptions.setPasswordExceptionsList = function(entries) {
    PasswordsExceptions.getInstance().setPasswordExceptionsList_(entries);
  };

  PasswordsExceptions.selectedPasswordCallback = function(password) {
    passwordsArea.displayReturnedPassword(password);
  };

  // Export
  return {
    PasswordsExceptions: PasswordsExceptions
  };

});

