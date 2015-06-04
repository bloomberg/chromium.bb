// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-user-list' shows a list of users whitelisted on this Chrome OS
 * device.
 *
 * Example:
 *
 *    <cr-settings-user-list prefs="{{prefs}}">
 *    </cr-settings-user-list>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-user-list
 */
Polymer({
  is: 'cr-settings-user-list',

  properties: {
    /**
     * Current list of whitelisted users.
     * @type {!Array<!User>}
     */
    users: {
      type: Array,
      value: function() { return []; },
      notify: true
    },
  },

  /** @override */
  ready: function() {
    chrome.settingsPrivate.onPrefsChanged.addListener(function(prefs) {
      prefs.forEach(function(pref) {
        if (pref.key == 'cros.accounts.users') {
          chrome.usersPrivate.getWhitelistedUsers(function(users) {
            this.users = users;
          }.bind(this));
        }
      }, this);
    });

    chrome.usersPrivate.getWhitelistedUsers(function(users) {
      this.users = users;
    }.bind(this));
  },

  /** @private */
  removeUser_: function(e) {
    chrome.usersPrivate.removeWhitelistedUser(
        e.model.item.email, /* callback */ function() {});
  }
});

