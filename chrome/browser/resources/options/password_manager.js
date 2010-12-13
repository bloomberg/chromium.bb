// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // PasswordManager class:

  /**
   * Encapsulated handling of password and exceptions page.
   * @constructor
   */
  function PasswordManager() {
    this.activeNavTab = null;
    OptionsPage.call(this,
                     'passwordManager',
                     templateData.savedPasswordsTitle,
                     'passwordManager');
  }

  cr.addSingletonGetter(PasswordManager);

  PasswordManager.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      options.passwordManager.PasswordsListArea.decorate($('passwordsArea'));
      options.passwordManager.PasswordExceptionsListArea.decorate(
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

  PasswordManager.load = function() {
    chrome.send('loadLists');
  };

  /**
   * Call to remove a saved password.
   * @param rowIndex indicating the row to remove.
   */
  PasswordManager.removeSavedPassword = function(rowIndex) {
      chrome.send('removeSavedPassword', [String(rowIndex)]);
  };

  /**
   * Call to remove a password exception.
   * @param rowIndex indicating the row to remove.
   */
  PasswordManager.removePasswordException = function(rowIndex) {
      chrome.send('removePasswordException', [String(rowIndex)]);
  };


  /**
   * Call to remove all saved passwords.
   * @param tab contentType of the tab currently on.
   */
  PasswordManager.removeAllPasswords = function() {
    chrome.send('removeAllSavedPasswords');
  };

  /**
   * Call to remove all saved passwords.
   * @param tab contentType of the tab currently on.
   */
  PasswordManager.removeAllPasswordExceptions = function() {
    chrome.send('removeAllPasswordExceptions');
  };

  PasswordManager.showSelectedPassword = function(index) {
    chrome.send('showSelectedPassword', [String(index)]);
  };

  PasswordManager.setSavedPasswordsList = function(entries) {
    PasswordManager.getInstance().setSavedPasswordsList_(entries);
  };

  PasswordManager.setPasswordExceptionsList = function(entries) {
    PasswordManager.getInstance().setPasswordExceptionsList_(entries);
  };

  PasswordManager.selectedPasswordCallback = function(password) {
    passwordsArea.displayReturnedPassword(password);
  };

  // Export
  return {
    PasswordManager: PasswordManager
  };

});

