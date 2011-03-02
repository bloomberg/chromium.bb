// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // PasswordManager class:

  /**
   * Encapsulated handling of password and exceptions page.
   * @constructor
   */
  function PasswordManager() {
    this.activeNavTab = null;
    OptionsPage.call(this,
                     'passwords',
                     templateData.passwordsPageTabTitle,
                     'password-manager');
  }

  cr.addSingletonGetter(PasswordManager);

  PasswordManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The saved passwords list.
     * @type {DeletableItemList}
     * @private
     */
    savedPasswordsList_: null,

    /**
     * The password exceptions list.
     * @type {DeletableItemList}
     * @private
     */
    passwordExceptionsList_: null,

    /** @inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createSavedPasswordsList_();
      this.createPasswordExceptionsList_();
    },

    /** @inheritDoc */
    canShowPage: function() {
      return !PersonalOptions.disablePasswordManagement();
    },

    /** @inheritDoc */
    didShowPage: function() {
      // Updating the password lists may cause a blocking platform dialog pop up
      // (Mac, Linux), so we delay this operation until the page is shown.
      chrome.send('updatePasswordLists');
    },

    /**
     * Creates, decorates and initializes the saved passwords list.
     * @private
     */
    createSavedPasswordsList_: function() {
      this.savedPasswordsList_ = $('saved-passwords-list');
      options.passwordManager.PasswordsList.decorate(this.savedPasswordsList_);
      this.savedPasswordsList_.autoExpands = true;
    },

    /**
     * Creates, decorates and initializes the password exceptions list.
     * @private
     */
    createPasswordExceptionsList_: function() {
      this.passwordExceptionsList_ = $('password-exceptions-list');
      options.passwordManager.PasswordExceptionsList.decorate(
          this.passwordExceptionsList_);
      this.passwordExceptionsList_.autoExpands = true;
    },

    /**
     * Updates the data model for the saved passwords list with the values from
     * |entries|.
     * @param {Array} entries The list of saved password data.
     */
    setSavedPasswordsList_: function(entries) {
      this.savedPasswordsList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Updates the data model for the password exceptions list with the values
     * from |entries|.
     * @param {Array} entries The list of password exception data.
     */
    setPasswordExceptionsList_: function(entries) {
      this.passwordExceptionsList_.dataModel = new ArrayDataModel(entries);
    },
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

  PasswordManager.setSavedPasswordsList = function(entries) {
    PasswordManager.getInstance().setSavedPasswordsList_(entries);
  };

  PasswordManager.setPasswordExceptionsList = function(entries) {
    PasswordManager.getInstance().setPasswordExceptionsList_(entries);
  };

  // Export
  return {
    PasswordManager: PasswordManager
  };

});

