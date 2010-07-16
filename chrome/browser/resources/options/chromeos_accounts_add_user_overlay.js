// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// AddUserOverlay class:

/**
 * Encapsulated handling of ChromeOS accounts add user overlay page.
 * @constructor
 */
function AddUserOverlay(model) {
  OptionsPage.call(this, 'addUserOverlay', localStrings.getString('add_user'),
                   'addUserOverlayPage');
}

AddUserOverlay.getInstance = function() {
  if (!AddUserOverlay.instance_) {
    AddUserOverlay.instance_ = new AddUserOverlay(null);
  }
  return AddUserOverlay.instance_;
};

AddUserOverlay.prototype = {
  // Inherit AddUserOverlay from OptionsPage.
  __proto__: OptionsPage.prototype,

  /**
   * Initializes AddUserOverlay page.
   * Calls base class implementation to starts preference initialization.
   */
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Set up the page.
    $('addUserOkButton').onclick = function(e) {
      var newUserEmail = $('userEmailEdit').value;
      if (newUserEmail)
        userList.addUser({'email': newUserEmail});

      OptionsPage.clearOverlays();
    };

    $('addUserCancelButton').onclick = function(e) {
      OptionsPage.clearOverlays();
    };

    this.addEventListener('visibleChange',
                          cr.bind(this.handleVisibleChange_, this));
  },

  /**
   * Handler for OptionsPage's visible property change event.
   * @param {Event} e Property change event.
   */
  handleVisibleChange_: function() {
    $('userEmailEdit').focus();
  }
};
