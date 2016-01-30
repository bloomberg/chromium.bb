// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-passwords-and-forms-page' is the settings page
 * for passwords and auto fill.
 *
 * @group Chrome Settings Elements
 * @element settings-passwords-and-forms-page
 */
(function() {
'use strict';

Polymer({
  is: 'settings-passwords-and-forms-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An array of passwords to display.
     * Lazy loaded when the password section is expanded.
     * @type {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Whether the password section section is opened or not.
     */
    passwordsOpened: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready: function() {
    // Triggers a callback after the listener is added.
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(
        function(passwordList) {
          this.savedPasswords = passwordList;
        }.bind(this));
  },
});
})();
