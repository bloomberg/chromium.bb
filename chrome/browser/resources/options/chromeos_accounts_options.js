// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// AccountsOptions class:

/**
 * Encapsulated handling of ChromeOS accounts options page.
 * @constructor
 */
function AccountsOptions(model) {
  OptionsPage.call(this, 'accounts', localStrings.getString('accountsPage'),
                   'accountsPage');
}

AccountsOptions.getInstance = function() {
  if (!AccountsOptions.instance_) {
    AccountsOptions.instance_ = new AccountsOptions(null);
  }
  return AccountsOptions.instance_;
};

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
    $('addUserButton').onclick = function(e) {
      OptionsPage.showOverlay('addUserOverlay');
    };
    $('removeUserButton').onclick = function(e) {
      $('userList').removeSelectedUser();
    };

    options.accounts.UserList.decorate($('userList'));

    this.addEventListener('visibleChange',
                          cr.bind(this.handleVisibleChange_, this));

    // Setup add user overlay page.
    OptionsPage.registerOverlay(AddUserOverlay.getInstance());
  },

  userListInitalized_: false,

  /**
   * Handler for OptionsPage's visible property change event.
   * @param {Event} e Property change event.
   */
  handleVisibleChange_ : function(e) {
    if (!this.userListInitalized_ && this.visible) {
      this.userListInitalized_ = true;
      userList.redraw();
    }
  }
};
