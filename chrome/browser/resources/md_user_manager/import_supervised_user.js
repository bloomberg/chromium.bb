// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'import-supervised-user' is a popup that allows user to select
 * a supervised profile from a list of profiles to import on the current device.
 */
(function() {
/**
 * It means no supervised user is selected.
 * @const {number}
 */
var NO_USER_SELECTED = -1;

Polymer({
  is: 'import-supervised-user',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * True if the element is currently hidden. The element toggles this in
     * order to display itself or hide itself once done.
     * @private {boolean}
     */
    popupHidden_: {
      type: Boolean,
      value: true
    },

    /**
     * The currently signed in user and the custodian.
     * @private {?SignedInUser}
     */
    signedInUser_: {
      type: Object,
      value: function() { return null; }
    },

    /**
     * The list of supervised users managed by signedInUser_.
     * @private {!Array<!SupervisedUser>}
     */
    supervisedUsers_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Index of the selected supervised user.
     * @private {number}
     */
    supervisedUserIndex_: {
      type: Number,
      value: NO_USER_SELECTED
    }
  },

  /**
   * Displays the popup.
   * @param {(!SignedInUser|undefined)} signedInUser
   * @param {!Array<!SupervisedUser>} supervisedUsers
   */
  show: function(signedInUser, supervisedUsers) {
    this.supervisedUsers_ = supervisedUsers;
    this.supervisedUsers_.sort(function(a, b) {
      if (a.onCurrentDevice != b.onCurrentDevice)
        return a.onCurrentDevice ? 1 : -1;
      return a.name.localeCompare(b.name);
    });

    this.supervisedUserIndex_ = NO_USER_SELECTED;

    this.signedInUser_ = signedInUser || null;
    if (this.signedInUser_)
      this.popupHidden_ = false;
  },

  /**
   * Computed binding that returns the appropriate import message depending on
   * whether or not there are any supervised users to import.
   * @param {!Array<!SupervisedUser>} supervisedUsers
   * @private
   * @return {string}
   */
  getMessage_: function(supervisedUsers) {
    return supervisedUsers.length > 0 ? this.i18n('supervisedUserImportText') :
                                        this.i18n('noSupervisedUserImportText');
  },

  /**
   * Computed binding that returns the appropriate class names for the HTML
   * container of |supervisedUser| depending on whether it is on this device.
   * @param {!SupervisedUser} supervisedUser
   * @private
   * @return {string}
   */
  getUserClassNames_: function(supervisedUser) {
    var classNames = 'list-item';
    if (!supervisedUser.onCurrentDevice)
      classNames += ' selectable';
    return classNames;
  },

  /**
   * Hides the popup.
   * @private
   */
  onCancelTap_: function() {
    this.popupHidden_ = true;
  },

  /**
   * Returns true if the 'Import' button should be enabled and false otherwise.
   * @private
   * @return {boolean}
   */
  isImportDisabled_: function(supervisedUserIndex) {
    return supervisedUserIndex == NO_USER_SELECTED;
  },

  /**
   * Called when the user clicks the 'Import' button. it proceeds with importing
   * the supervised user.
   * @private
   */
  onImportTap_: function() {
    var supervisedUser = this.supervisedUsers_[this.supervisedUserIndex_];
    if (this.signedInUser_ && supervisedUser) {
      // Event is caught by create-profile.
      this.fire('import', {supervisedUser: supervisedUser,
                           signedInUser: this.signedInUser_});
      this.popupHidden_ = true;
    }
  }
});
})();
