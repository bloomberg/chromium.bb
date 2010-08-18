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

      var areas = document.querySelectorAll(
          '#passwordsExceptionsPage div[contentType]');
      for (var i = 0; i < areas.length; i++) {
        options.passwordsExceptions.ListArea.decorate(areas[i]);
      }
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

  /**
   * Call to remove a row.
   * @param tab contentType of the tab currently on.
   * @param rowIndex indicating the row to remove.
   */
  PasswordsExceptions.removeEntry = function(tab, rowIndex) {
    if(tab == 'passwords')
      chrome.send('removeSavedPassword', [String(rowIndex)]);
    else
      chrome.send('removePasswordsException', [String(rowIndex)]);
  };

  /**
   * Call to remove all saved passwords or passwords exceptions.
   * @param tab contentType of the tab currently on.
   */
  PasswordsExceptions.removeAll = function(tab) {
    if(tab == 'passwords')
      chrome.send('removeAllSavedPasswords');
    else
      chrome.send('removeAllPasswordsExceptions')
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

