// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // AccountsOptions class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function AccountsOptions(model) {
    OptionsPage.call(this, 'accounts', templateData.accountsPageTabTitle,
                     'accountsPage');
  }

  cr.addSingletonGetter(AccountsOptions);

  AccountsOptions.prototype = {
    // Inherit AccountsOptions from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes AccountsOptions page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up accounts page.
      var userList = $('userList');

      var userNameEdit = $('userNameEdit');
      options.accounts.UserNameEdit.decorate(userNameEdit);
      userNameEdit.addEventListener('add', this.handleAddUser_);

      // If the current user is not the owner, show some warning,
      // and do not show the user list.
      if (AccountsOptions.currentUserIsOwner()) {
        options.accounts.UserList.decorate(userList);
      } else {
        if (!AccountsOptions.whitelistIsManaged()) {
          $('ownerOnlyWarning').hidden = false;
        } else {
          this.managed = true;
        }
      }

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      $('useWhitelistCheck').addEventListener('change',
          this.handleUseWhitelistCheckChange_.bind(this));

      Preferences.getInstance().addEventListener(
          $('useWhitelistCheck').pref,
          this.handleUseWhitelistPrefChange_.bind(this));
    },

    /**
     * Update user list control state.
     * @private
     */
    updateControls_: function() {
      $('userList').disabled =
      $('userNameEdit').disabled = !AccountsOptions.currentUserIsOwner() ||
                                   AccountsOptions.whitelistIsManaged() ||
                                   !$('useWhitelistCheck').checked;
    },

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (this.visible) {
        this.updateControls_();
        if (AccountsOptions.currentUserIsOwner())
          $('userList').redraw();
      }
    },

    /**
     * Handler for allow guest check change.
     * @private
     */
    handleUseWhitelistCheckChange_: function(e) {
      // Whitelist existing users when guest login is being disabled.
      if ($('useWhitelistCheck').checked) {
        chrome.send('whitelistExistingUsers', []);
      }

      this.updateControls_();
    },

    /**
     * handler for allow guest pref change.
     * @private
     */
    handleUseWhitelistPrefChange_: function(e) {
      this.updateControls_();
    },

    /**
     * Handler for "add" event fired from userNameEdit.
     * @private
     * @param {Event} e Add event fired from userNameEdit.
     */
    handleAddUser_: function(e) {
      AccountsOptions.addUsers([e.user]);
    }
  };

  /**
   * Returns whether the current user is owner or not.
   */
  AccountsOptions.currentUserIsOwner = function() {
    return localStrings.getString('current_user_is_owner') == 'true';
  };

  /**
   * Returns whether we're currently in guest mode.
   */
  AccountsOptions.loggedInAsGuest = function() {
    return localStrings.getString('logged_in_as_guest') == 'true';
  };

  /**
   * Returns whether the whitelist is managed by policy or not.
   */
  AccountsOptions.whitelistIsManaged = function() {
    return localStrings.getString('whitelist_is_managed') == 'true';
  };

  /**
   * Adds given users to userList.
   */
  AccountsOptions.addUsers = function(users) {
    var userList = $('userList');
    for (var i = 0; i < users.length; ++i) {
      userList.addUser(users[i]);
    }
  };

  /**
   * Update account picture.
   * @param {string} email Email of the user to update.
   */
  AccountsOptions.updateAccountPicture = function(email) {
    if (this.currentUserIsOwner())
      $('userList').updateAccountPicture(email);
  };

  // Export
  return {
    AccountsOptions: AccountsOptions
  };

});
