// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-users-page' is the settings page for managing user accounts on
 * the device.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <cr-settings-users-page prefs="{{prefs}}">
 *      </cr-settings-users-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-users-page
 */
Polymer({
  is: 'cr-settings-users-page',

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

    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'users',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('usersPageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'person',
      readOnly: true,
    },

    /** @override */
    keyEventTarget: {
      type: Object,
      value: function() {
        return this.$.addUserInput;
      }
    }
  },

  keyBindings: {
    'enter': 'addUser_'
  },

  /** @private */
  addUser_: function() {
    // TODO(orenb): Validate before adding.
    chrome.usersPrivate.addWhitelistedUser(
        this.$.addUserInput.value, /* callback */ function() {});
    this.$.addUserInput.value = '';
  }
});
