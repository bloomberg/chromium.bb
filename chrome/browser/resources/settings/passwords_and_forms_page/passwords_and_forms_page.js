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
      observer: 'loadPasswords_',
    },
  },

  /**
   * Called when the section is expanded. This will load the list of passwords
   * only when needed.
   * @param {boolean} passwordSectionOpened
   */
  loadPasswords_: function(passwordSectionOpened) {
    if (passwordSectionOpened) {
      // TODO(hcarmona): Get real data.
      this.savedPasswords =
          [{origin: 'otherwebsite.com',
            username: 'bowser',
            password: '************'},
           {origin: 'otherlongwebsite.com',
            username: 'koopa',
            password: '*********'},
           {origin: 'otherverylongwebsite.com',
            username: 'goomba',
            password: '******'}];
    }
  },
});
})();
