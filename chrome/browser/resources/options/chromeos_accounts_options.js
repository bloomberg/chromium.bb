// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'accounts', localStrings.getString('accountsPage'),
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

      userList.disabled =
      userNameEdit.disabled = !AccountsOptions.currentUserIsOwner();
      // If the current user is not the owner, show some warning,
      // and do not show the user list.
      if (AccountsOptions.currentUserIsOwner()) {
        options.accounts.UserList.decorate(userList);
      } else {
        $('ownerOnlyWarning').classList.remove('hidden');
      }

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      $('useWhitelistCheck').addEventListener('click',
          this.handleUseWhitelistCheckClick_);
    },

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (this.visible) {
        // fetchUserPictures calls back AccountsOptions.setUserPictures and
        // triggers redraw.
        chrome.send('fetchUserPictures', []);
      }
    },

    /**
     * Handler for allow guest check click.
     * @private
     */
    handleUseWhitelistCheckClick_: function(e) {
      // Whitelist existing users when guest login is being disabled.
      if ($('useWhitelistCheck').checked) {
        chrome.send('whitelistExistingUsers', []);
      }
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
   * Updates user picture cache in UserList.
   */
  AccountsOptions.setUserPictures = function(cache) {
    $('userList').setUserPictures(cache);
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

  // Export
  return {
    AccountsOptions: AccountsOptions
  };

});
