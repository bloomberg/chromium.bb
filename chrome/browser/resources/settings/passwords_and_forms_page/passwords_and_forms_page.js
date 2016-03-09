// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-passwords-and-forms-page' is the settings page
 * for passwords and auto fill.
 */

/**
 * Interface for all callbacks to the password API.
 * @interface
 */
function PasswordManager() {}

/** @typedef {chrome.passwordsPrivate.PasswordUiEntry} */
PasswordManager.PasswordUiEntry;

PasswordManager.prototype = {
  /**
   * Register a callback for when the list of passwords is updated.
   * Calling this function should trigger an update.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} callback
   */
  onSavedPasswordListChangedCallback: assertNotReached,

  /**
   * Register a callback for when the list of exceptions is updated.
   * Calling this function should trigger an update.
   * @param {function(!Array<!string>):void} callback
   */
  onExceptionListChangedCallback: assertNotReached,

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {!string} exception The exception that should be removed from the
   *     list. No-op if |exception| is not in the list.
   */
  removePasswordException: assertNotReached,
};

/**
 * Implementation that accesses the private API.
 * @implements {PasswordManager}
 * @constructor
 */
function PasswordManagerImpl() {}
cr.addSingletonGetter(PasswordManagerImpl);

PasswordManagerImpl.prototype = {
  __proto__: PasswordManager,

  /** @override */
  onSavedPasswordListChangedCallback: function(callback) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
  },

  /** @override */
  onExceptionListChangedCallback: function(callback) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
  },

  /** @override */
  removePasswordException: function(exception) {
    chrome.passwordsPrivate.removePasswordException(exception);
  },
};

(function() {
'use strict';

Polymer({
  is: 'settings-passwords-and-forms-page',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An array of passwords to display.
     * @type {!Array<!PasswordManager.PasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * An array of sites to display.
     * @type {!Array<!string>}
     */
    passwordExceptions: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Whether the password section section is opened or not.
     * @type {boolean}
     */
    passwordsOpened: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'remove-password-exception': 'removePasswordException_'
  },

  /** @override */
  ready: function() {
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    this.passwordManager_.onSavedPasswordListChangedCallback(function(list) {
      this.savedPasswords = list;
    }.bind(this));
    this.passwordManager_.onExceptionListChangedCallback(function(list) {
      this.passwordExceptions = list;
    }.bind(this));
  },

  /**
   * Listens for the remove-password-exception event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  removePasswordException_: function(event) {
    this.passwordManager_.removePasswordException(event.detail);
  },
});
})();
