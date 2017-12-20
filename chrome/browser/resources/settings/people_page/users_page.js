// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 */
Polymer({
  is: 'settings-users-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * True if the current profile manages supervised users.
     * Set in people-page.
     */
    profileManagesSupervisedUsers: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isOwner_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isWhitelistManaged_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  created: function() {
    chrome.usersPrivate.isCurrentUserOwner(isOwner => {
      this.isOwner_ = isOwner;
    });

    chrome.usersPrivate.isWhitelistManaged(isWhitelistManaged => {
      this.isWhitelistManaged_ = isWhitelistManaged;
    });
  },

  /**
   * @param {!Event} e
   * @private
   */
  openAddUserDialog_: function(e) {
    e.preventDefault();
    this.$.addUserDialog.open();
  },

  /** @private */
  onAddUserDialogClose_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#add-user-button a')));
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   * @return {boolean}
   */
  isEditingDisabled_: function(isOwner, isWhitelistManaged) {
    return !isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @param {boolean} allowGuest
   * @private
   * @return {boolean}
   */
  isEditingUsersDisabled_: function(isOwner, isWhitelistManaged, allowGuest) {
    return !isOwner || isWhitelistManaged || allowGuest;
  }
});
