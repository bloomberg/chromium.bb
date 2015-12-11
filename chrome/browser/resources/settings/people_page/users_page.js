// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <settings-users-page prefs="{{prefs}}">
 *      </settings-users-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-users-page
 */
Polymer({
  is: 'settings-users-page',

  behaviors: [
    Polymer.IronA11yKeysBehavior
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @override */
    keyEventTarget: {
      type: Object,
      value: function() {
        return this.$.addUserInput;
      }
    },

    isOwner: {
      type: Boolean,
      value: false
    },

    isWhitelistManaged: {
      type: Boolean,
      value: false
    },

    editingDisabled: {
      type: Boolean,
      computed: 'computeEditingDisabled_(isOwner, isWhitelistManaged)'
    },

    editingUsersDisabled: {
      type: Boolean,
      computed: 'computeEditingUsersDisabled_(isOwner, isWhitelistManaged, ' +
          'prefs.cros.accounts.allowGuest.value)'
    }
  },

  keyBindings: {
    'enter': 'addUser_'
  },

  /** @override */
  created: function() {
    chrome.usersPrivate.isCurrentUserOwner(function(isOwner) {
      this.isOwner = isOwner;
    }.bind(this));

    chrome.usersPrivate.isWhitelistManaged(function(isWhitelistManaged) {
      this.isWhitelistManaged = isWhitelistManaged;
    }.bind(this));
  },

  /**
   * Regular expression for adding a user where the string provided is just the
   * part before the "@".
   * Email alias only, assuming it's a gmail address.
   *     e.g. 'john'
   * @const
   * @private {string}
   */
  nameOnlyString_: '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$',

  /**
   * Regular expression for adding a user where the string provided is a full
   * email address.
   *     e.g. 'john@chromium.org'
   * @const
   * @private {string}
   */
  emailString_:
      '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
      '([A-Za-z0-9\-]{2,63}\\..+)\\s*$',

  /** @private */
  addUser_: function() {
    /** @const */ var nameOnlyRegex = new RegExp(this.nameOnlyString_);
    /** @const */ var emailRegex = new RegExp(this.emailString_);

    var userStr = this.$.addUserInput.value;

    var matches = nameOnlyRegex.exec(userStr);
    var userEmail;
    if (matches) {
      userEmail = matches[1] + '@gmail.com';
    }

    matches = emailRegex.exec(userStr);
    if (matches) {
      userEmail = matches[1] + '@' + matches[2];
    }

    if (userEmail) {
      chrome.usersPrivate.addWhitelistedUser(
          userEmail,
          /* callback */ function(success) {});
      this.$.addUserInput.value = '';
    }
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   */
  computeHideOwnerLabel_: function(isOwner, isWhitelistManaged) {
    return isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   */
  computeHideManagedLabel_: function(isOwner, isWhitelistManaged) {
    return !isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   */
  computeEditingDisabled_: function(isOwner, isWhitelistManaged) {
    return !isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @param {boolean} allowGuest
   * @private
   */
  computeEditingUsersDisabled_: function(
      isOwner, isWhitelistManaged, allowGuest) {
    return !isOwner || isWhitelistManaged || allowGuest;
  }
});
