// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
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

/** @typedef {chrome.passwordsPrivate.ExceptionPair} */
PasswordManager.ExceptionPair;

/** @typedef {chrome.passwordsPrivate.PlaintextPasswordEventParameters} */
PasswordManager.PlaintextPasswordEvent;

PasswordManager.prototype = {
  /**
   * Add an observer to the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} listener
   */
  addSavedPasswordListChangedListener: assertNotReached,

  /**
   * Remove an observer from the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} listener
   */
  removeSavedPasswordListChangedListener: assertNotReached,

  /**
   * Request the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} callback
   */
  getSavedPasswordList: assertNotReached,

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param {!PasswordManager.LoginPair} loginPair The saved password that
   *     should be removed from the list. No-op if |loginPair| is not found.
   */
  removeSavedPassword: assertNotReached,

  /**
   * Add an observer to the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionPair>):void} listener
   */
  addExceptionListChangedListener: assertNotReached,

  /**
   * Remove an observer from the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionPair>):void} listener
   */
  removeExceptionListChangedListener: assertNotReached,

  /**
   * Request the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionPair>):void} callback
   */
  getExceptionList: assertNotReached,

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {string} exception The exception that should be removed from the
   *     list. No-op if |exception| is not in the list.
   */
  removeException: assertNotReached,

  /**
   * Gets the saved password for a given login pair.
   * @param {!PasswordManager.LoginPair} loginPair The saved password that
   *     should be retrieved.
   * @param {function(!PasswordManager.PlaintextPasswordEvent):void} callback
   */
  getPlaintextPassword: assertNotReached,
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
  addSavedPasswordListChangedListener: function(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  },

  /** @override */
  removeSavedPasswordListChangedListener: function(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  },

  /** @override */
  getSavedPasswordList: function(callback) {
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  /** @override */
  removeSavedPassword: function(loginPair) {
    chrome.passwordsPrivate.removeSavedPassword(loginPair);
  },

  /** @override */
  addExceptionListChangedListener: function(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  },

  /** @override */
  removeExceptionListChangedListener: function(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
        listener);
  },

  /** @override */
  getExceptionList: function(callback) {
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  /** @override */
  removeException: function(exception) {
    chrome.passwordsPrivate.removePasswordException(exception);
  },

  /** @override */
  getPlaintextPassword: function(loginPair, callback) {
    var listener = function(reply) {
      // Only handle the reply for our loginPair request.
      if (reply.loginPair.originUrl == loginPair.originUrl &&
          reply.loginPair.username == loginPair.username) {
        chrome.passwordsPrivate.onPlaintextPasswordRetrieved.removeListener(
            listener);
        callback(reply);
      }
    };
    chrome.passwordsPrivate.onPlaintextPasswordRetrieved.addListener(listener);
    chrome.passwordsPrivate.requestPlaintextPassword(loginPair);
  },
};

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.PasswordUiEntry}}} */
var PasswordUiEntryEvent;

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionPair}}} */
var ExceptionPairEntryEvent;

(function() {
'use strict';

Polymer({
  is: 'passwords-section',

  behaviors: [settings.GlobalScrollTargetBehavior],

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
    savedPasswords: Array,

    /**
     * An array of sites to display.
     * @type {!Array<!PasswordManager.ExceptionPair>}
     */
    passwordExceptions: Array,

    /** @override */
    subpageRoute: {
      type: Object,
      value: settings.Route.MANAGE_PASSWORDS,
    },

    /**
     * The model for any password related action menus or dialogs.
     * @private {?chrome.passwordsPrivate.PasswordUiEntry}
     */
    activePassword: Object,

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'show-password': 'showPassword_',
  },

  /**
   * @type {PasswordManager}
   * @private
   */
  passwordManager_: null,

  /**
   * @type {?function(!Array<PasswordManager.PasswordUiEntry>):void}
   * @private
   */
  setSavedPasswordsListener_: null,

  /**
   * @type {?function(!Array<PasswordManager.ExceptionPair>):void}
   * @private
   */
  setPasswordExceptionsListener_: null,

  /** @override */
  ready: function() {
    // Create listener functions.
    var setSavedPasswordsListener = function(list) {
      this.savedPasswords = list;
    }.bind(this);

    var setPasswordExceptionsListener = function(list) {
      this.passwordExceptions = list;
    }.bind(this);

    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.setPasswordExceptionsListener_ = setPasswordExceptionsListener;

    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    // Request initial data.
    this.passwordManager_.getSavedPasswordList(setSavedPasswordsListener);
    this.passwordManager_.getExceptionList(setPasswordExceptionsListener);

    // Listen for changes.
    this.passwordManager_.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);
    this.passwordManager_.addExceptionListChangedListener(
        setPasswordExceptionsListener);
  },

  /** @override */
  detached: function() {
    this.passwordManager_.removeSavedPasswordListChangedListener(
      /** @type {function(!Array<PasswordManager.PasswordUiEntry>):void} */(
        this.setSavedPasswordsListener_));
    this.passwordManager_.removeExceptionListChangedListener(
      /** @type {function(!Array<PasswordManager.ExceptionPair>):void} */(
        this.setPasswordExceptionsListener_));
  },

  /**
   * Sets the password in the current password dialog if the loginPair matches.
   * @param {!chrome.passwordsPrivate.LoginPair} loginPair
   * @param {string} password
   */
  setPassword: function(loginPair, password) {
    if (this.activePassword &&
        this.activePassword.loginPair.originUrl == loginPair.originUrl &&
        this.activePassword.loginPair.username == loginPair.username) {
      this.$$('password-edit-dialog').password = password;
    }
  },

  /**
   * Shows the edit password dialog.
   * @param {!Event} e
   * @private
   */
  onMenuEditPasswordTap_: function(e) {
    e.preventDefault();
    /** @type {CrActionMenuElement} */(this.$.menu).close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_: function() {
    this.showPasswordEditDialog_ = false;
  },

  /**
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} savedPasswords
   * @param {string} filter
   * @return {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_: function(savedPasswords, filter) {
    if (!filter)
      return savedPasswords;

    return savedPasswords.filter(function(password) {
      return password.loginPair.originUrl.includes(filter) ||
             password.loginPair.username.includes(filter);
    });
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionPair): boolean}
   * @private
   */
  passwordExceptionFilter_: function(filter) {
    return function(exception) {
      return exception.exceptionUrl.includes(filter);
    };
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_: function() {
    this.passwordManager_.removeSavedPassword(this.activePassword.loginPair);
    /** @type {CrActionMenuElement} */(this.$.menu).close();
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!ExceptionPairEntryEvent} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_: function(e) {
    this.passwordManager_.removeException(e.model.item.exceptionUrl);
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_: function(length) { return ' '.repeat(length); },

  /**
   * Opens the password action menu.
   * @private
   */
  onPasswordMenuTap_: function(e) {
    var menu = /** @type {!CrActionMenuElement} */(this.$.menu);
    var target = /** @type {!Element} */(Polymer.dom(e).localTarget);
    var passwordUiEntryEvent = /** @type {!PasswordUiEntryEvent} */(e);

    this.activePassword =
        /** @type {!chrome.passwordsPrivate.PasswordUiEntry} */ (
            passwordUiEntryEvent.model.item);
    menu.showAt(target);
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_: function(list) {
    return !!(list && list.length);
  },

  /**
   * Listens for the show-password event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  showPassword_: function(event) {
    this.passwordManager_.getPlaintextPassword(
        /** @type {!PasswordManager.LoginPair} */(event.detail),
        function(item) {
          this.setPassword(item.loginPair, item.plaintextPassword);
        }.bind(this));
  },
});
})();
