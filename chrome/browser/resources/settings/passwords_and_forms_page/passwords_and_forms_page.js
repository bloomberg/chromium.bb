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

/** @typedef {chrome.passwordsPrivate.LoginPair} */
PasswordManager.LoginPair;

/** @typedef {chrome.passwordsPrivate.PlaintextPasswordEventParameters} */
PasswordManager.PlaintextPasswordEvent;

PasswordManager.prototype = {
  /**
   * Request the list of saved passwords and observe future changes.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} callback
   */
  setSavedPasswordListChangedCallback: assertNotReached,

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param {!PasswordManager.LoginPair} loginPair The saved password that
   *     should be removed from the list. No-op if |loginPair| is not found.
   */
  removeSavedPassword: assertNotReached,

  /**
   * Request the list of password exceptions and observe future changes.
   * @param {function(!Array<!string>):void} callback
   */
  setExceptionListChangedCallback: assertNotReached,

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {!string} exception The exception that should be removed from the
   *     list. No-op if |exception| is not in the list.
   */
  removePasswordException: assertNotReached,

  /**
   * Register a callback for when a password is requested.
   * @param {function(!Array<!PasswordManager.LoginPair>):void} callback
   */
  onPlaintextPasswordRequestedCallback: assertNotReached,

  /**
   * Should request the saved password for a given login pair.
   * @param {!PasswordManager.LoginPair} loginPair The saved password that
   *     should be retrieved.
   */
  requestPlaintextPassword: assertNotReached,
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
  setSavedPasswordListChangedCallback: function(callback) {
    // Get the list of passwords...
    chrome.passwordsPrivate.getSavedPasswordList(callback);
    // ...and listen for future changes.
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
  },

  /** @override */
  removeSavedPassword: function(loginPair) {
    chrome.passwordsPrivate.removeSavedPassword(loginPair);
  },

  /** @override */
  setExceptionListChangedCallback: function(callback) {
    // Get the list of exceptions...
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
    // ...and listen for future changes.
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
  },

  /** @override */
  removePasswordException: function(exception) {
    chrome.passwordsPrivate.removePasswordException(exception);
  },

  /** @override */
  onPlaintextPasswordRequestedCallback: function(callback) {
    chrome.passwordsPrivate.onPlaintextPasswordRetrieved.addListener(callback);
  },

  /** @override */
  requestPlaintextPassword: function(loginPair) {
    chrome.passwordsPrivate.requestPlaintextPassword(loginPair);
  },
};

(function() {
'use strict';

Polymer({
  is: 'settings-passwords-and-forms-page',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** The current active route. */
    currentRoute: {
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
  },

  listeners: {
    'remove-password-exception': 'removePasswordException_',
    'remove-saved-password': 'removeSavedPassword_',
    'show-password': 'showPassword_',
  },

  /** @override */
  ready: function() {
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    this.passwordManager_.setSavedPasswordListChangedCallback(function(list) {
      this.savedPasswords = list;
    }.bind(this));
    this.passwordManager_.setExceptionListChangedCallback(function(list) {
      this.passwordExceptions = list;
    }.bind(this));
    this.passwordManager_.onPlaintextPasswordRequestedCallback(function(e) {
      this.$$('#passwordSection').setPassword(e.loginPair, e.plaintextPassword);
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

  /**
   * Listens for the remove-saved-password event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  removeSavedPassword_: function(event) {
    this.passwordManager_.removeSavedPassword(event.detail);
  },

  /**
   * Shows the manage passwords sub page.
   * @param {!Event} event
   * @private
   */
  onPasswordsTap_: function(event) {
    // Ignore clicking on the toggle button and only expand if the manager is
    // enabled.
    if (Polymer.dom(event).localTarget != this.$.passwordToggle &&
        this.getPref('profile.password_manager_enabled').value) {
      this.$.pages.setSubpageChain(['manage-passwords']);
    }
  },

  /**
   * Listens for the show-password event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  showPassword_: function(event) {
    this.passwordManager_.requestPlaintextPassword(event.detail);
  },
});
})();
