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
   * @param {!string} exception The exception that should be removed from the
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
 * Interface for all callbacks to the autofill API.
 * @interface
 */
function AutofillManager() {}

/** @typedef {chrome.autofillPrivate.AddressEntry} */
AutofillManager.AddressEntry;

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
AutofillManager.CreditCardEntry;

AutofillManager.prototype = {
  /**
   * Add an observer to the list of addresses.
   * @param {function(!Array<!AutofillManager.AddressEntry>):void} listener
   */
  addAddressListChangedListener: assertNotReached,

  /**
   * Remove an observer from the list of addresses.
   * @param {function(!Array<!AutofillManager.AddressEntry>):void} listener
   */
  removeAddressListChangedListener: assertNotReached,

  /**
   * Request the list of addresses.
   * @param {function(!Array<!AutofillManager.AddressEntry>):void} callback
   */
  getAddressList: assertNotReached,

  /**
   * Add an observer to the list of credit cards.
   * @param {function(!Array<!AutofillManager.CreditCardEntry>):void} listener
   */
  addCreditCardListChangedListener: assertNotReached,

  /**
   * Remove an observer from the list of credit cards.
   * @param {function(!Array<!AutofillManager.CreditCardEntry>):void} listener
   */
  removeCreditCardListChangedListener: assertNotReached,

  /**
   * Request the list of credit cards.
   * @param {function(!Array<!AutofillManager.CreditCardEntry>):void} callback
   */
  getCreditCardList: assertNotReached,
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

/**
 * Implementation that accesses the private API.
 * @implements {AutofillManager}
 * @constructor
 */
function AutofillManagerImpl() {}
cr.addSingletonGetter(AutofillManagerImpl);

AutofillManagerImpl.prototype = {
  __proto__: AutofillManager,

  /** @override */
  addAddressListChangedListener: function(listener) {
    chrome.autofillPrivate.onAddressListChanged.addListener(listener);
  },

  /** @override */
  removeAddressListChangedListener: function(listener) {
    chrome.autofillPrivate.onAddressListChanged.removeListener(listener);
  },

  /** @override */
  getAddressList: function(callback) {
    chrome.autofillPrivate.getAddressList(callback);
  },

  /** @override */
  addCreditCardListChangedListener: function(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.addListener(listener);
  },

  /** @override */
  removeCreditCardListChangedListener: function(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.removeListener(listener);
  },

  /** @override */
  getCreditCardList: function(callback) {
    chrome.autofillPrivate.getCreditCardList(callback);
  },
};

(function() {
'use strict';

Polymer({
  is: 'settings-passwords-and-forms-page',

  behaviors: [
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
    },

    /**
     * An array of sites to display.
     * @type {!Array<!PasswordManager.ExceptionPair>}
     */
    passwordExceptions: {
      type: Array,
    },

     /**
     * An array of saved addresses.
     * @type {!Array<!AutofillManager.AddressEntry>}
     */
    addresses: {
      type: Array,
    },

    /**
     * An array of saved addresses.
     * @type {!Array<!AutofillManager.CreditCardEntry>}
     */
    creditCards: {
      type: Array,
    },
 },

  listeners: {
    'remove-password-exception': 'removePasswordException_',
    'remove-saved-password': 'removeSavedPassword_',
    'show-password': 'showPassword_',
  },

  /** @type {?function(!Array<PasswordManager.PasswordUiEntry>):void} */
  setSavedPasswordsListener_: null,

  /** @type {?function(!Array<PasswordManager.ExceptionPair>):void} */
  setPasswordExceptionsListener_: null,

  /** @type {?function(!Array<!AutofillManager.AddressEntry>)} */
  setAddressesListener_: null,

  /** @type {?function(!Array<!AutofillManager.CreditCardEntry>)} */
  setCreditCardsListener_: null,

  /** @override */
  ready: function() {
    // Create listener functions.
    this.setSavedPasswordsListener_ = function(list) {
      this.savedPasswords = list;
    }.bind(this);

    this.setPasswordExceptionsListener_ = function(list) {
      this.passwordExceptions = list;
    }.bind(this);

    this.setAddressesListener_ = function(list) {
      this.addresses = list;
    }.bind(this);

    this.setCreditCardsListener_ = function(list) {
      this.creditCards = list;
    }.bind(this);

    // Set the managers. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
    this.autofillManager_ = AutofillManagerImpl.getInstance();

    // Request initial data.
    this.passwordManager_.getSavedPasswordList(this.setSavedPasswordsListener_);
    this.passwordManager_.getExceptionList(this.setPasswordExceptionsListener_);
    this.autofillManager_.getAddressList(this.setAddressesListener_);
    this.autofillManager_.getCreditCardList(this.setCreditCardsListener_);

    // Listen for changes.
    this.passwordManager_.addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.passwordManager_.addExceptionListChangedListener(
        this.setPasswordExceptionsListener_);
    this.autofillManager_.addAddressListChangedListener(
        this.setAddressesListener_);
    this.autofillManager_.addCreditCardListChangedListener(
        this.setCreditCardsListener_);
  },

  /** @override */
  detached: function() {
    this.passwordManager_.removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.passwordManager_.removeExceptionListChangedListener(
        this.setPasswordExceptionsListener_);
    this.autofillManager_.removeAddressListChangedListener(
        this.setAddressesListener_);
    this.autofillManager_.removeCreditCardListChangedListener(
        this.setCreditCardsListener_);
  },

  /**
   * Listens for the remove-password-exception event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  removePasswordException_: function(event) {
    this.passwordManager_.removeException(event.detail);
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
   * Shows the manage autofill sub page.
   * @param {!Event} event
   * @private
   */
  onAutofillTap_: function(event) {
    // Ignore clicking on the toggle button and verify autofill is enabled.
    if (Polymer.dom(event).localTarget != this.$.autofillToggle &&
        this.getPref('autofill.enabled').value) {
      this.$.pages.setSubpageChain(['manage-autofill']);
    }
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
    this.passwordManager_.getPlaintextPassword(event.detail, function(e) {
      this.$$('#passwordSection').setPassword(e.loginPair, e.plaintextPassword);
    }.bind(this));
  },
});
})();
