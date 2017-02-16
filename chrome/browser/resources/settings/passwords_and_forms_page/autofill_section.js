// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses and credit cards for use in autofill.
 */

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
   * Saves the given address.
   * @param {!AutofillManager.AddressEntry} address
   */
  saveAddress: assertNotReached,

  /** @param {string} guid The guid of the address to remove.  */
  removeAddress: assertNotReached,

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

  /** @param {string} guid The GUID of the credit card to remove.  */
  removeCreditCard: assertNotReached,

  /** @param {string} guid The GUID to credit card to remove from the cache. */
  clearCachedCreditCard: assertNotReached,

  /**
   * Saves the given credit card.
   * @param {!AutofillManager.CreditCardEntry} creditCard
   */
  saveCreditCard: assertNotReached,
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
  saveAddress: function(address) {
    chrome.autofillPrivate.saveAddress(address);
  },

  /** @override */
  removeAddress: function(guid) {
    chrome.autofillPrivate.removeEntry(assert(guid));
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

  /** @override */
  removeCreditCard: function(guid) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  },

  /** @override */
  clearCachedCreditCard: function(guid) {
    chrome.autofillPrivate.maskCreditCard(assert(guid));
  },

  /** @override */
  saveCreditCard: function(creditCard) {
    chrome.autofillPrivate.saveCreditCard(creditCard);
  }
};

(function() {
  'use strict';

  Polymer({
    is: 'settings-autofill-section',

    behaviors: [I18nBehavior],

    properties: {
      /**
       * An array of saved addresses.
       * @type {!Array<!AutofillManager.AddressEntry>}
       */
      addresses: Array,

      /**
       * The model for any address related action menus or dialogs.
       * @private {?chrome.autofillPrivate.AddressEntry}
       */
      activeAddress: Object,

      /** @private */
      showAddressDialog_: Boolean,

      /**
       * An array of saved credit cards.
       * @type {!Array<!AutofillManager.CreditCardEntry>}
       */
      creditCards: Array,

      /**
       * The model for any credit card related action menus or dialogs.
       * @private {?chrome.autofillPrivate.CreditCardEntry}
       */
      activeCreditCard: Object,

      /** @private */
      showCreditCardDialog_: Boolean,
    },

    listeners: {
      'save-address': 'saveAddress_',
      'save-credit-card': 'saveCreditCard_',
    },

    /**
     * @type {AutofillManager}
     * @private
     */
    autofillManager_: null,

    /**
     * @type {?function(!Array<!AutofillManager.AddressEntry>)}
     * @private
     */
    setAddressesListener_: null,

    /**
     * @type {?function(!Array<!AutofillManager.CreditCardEntry>)}
     * @private
     */
    setCreditCardsListener_: null,

    /** @override */
    ready: function() {
      // Create listener functions.
      /** @type {function(!Array<!AutofillManager.AddressEntry>)} */
      var setAddressesListener = function(list) {
        this.addresses = list;
      }.bind(this);

      /** @type {function(!Array<!AutofillManager.CreditCardEntry>)} */
      var setCreditCardsListener = function(list) {
        this.creditCards = list;
      }.bind(this);

      // Remember the bound reference in order to detach.
      this.setAddressesListener_ = setAddressesListener;
      this.setCreditCardsListener_ = setCreditCardsListener;

      // Set the managers. These can be overridden by tests.
      this.autofillManager_ = AutofillManagerImpl.getInstance();

      // Request initial data.
      this.autofillManager_.getAddressList(setAddressesListener);
      this.autofillManager_.getCreditCardList(setCreditCardsListener);

      // Listen for changes.
      this.autofillManager_.addAddressListChangedListener(setAddressesListener);
      this.autofillManager_.addCreditCardListChangedListener(
          setCreditCardsListener);
    },

    /** @override */
    detached: function() {
      this.autofillManager_.removeAddressListChangedListener(
          /** @type {function(!Array<!AutofillManager.AddressEntry>)} */(
          this.setAddressesListener_));
      this.autofillManager_.removeCreditCardListChangedListener(
          /** @type {function(!Array<!AutofillManager.CreditCardEntry>)} */(
          this.setCreditCardsListener_));
    },

    /**
     * Formats the expiration date so it's displayed as MM/YYYY.
     * @param {!chrome.autofillPrivate.CreditCardEntry} item
     * @return {string}
     * @private
     */
    expiration_: function(item) {
      return item.expirationMonth + '/' + item.expirationYear;
    },

    /**
     * Open the address action menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onAddressMenuTap_: function(e) {
      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);

      /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
       https://github.com/Polymer/polymer/issues/2574 */
      var item = menuEvent.model['dataHost']['dataHost'].item;

      // Copy item so dialog won't update model on cancel.
      this.activeAddress = /** @type {!chrome.autofillPrivate.AddressEntry} */(
          Object.assign({}, item));

      var dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
      /** @type {!CrActionMenuElement} */ (
          this.$.addressSharedMenu).showAt(dotsButton);
    },

    /**
     * Handles tapping on the "Add address" button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onAddAddressTap_: function(e) {
      e.preventDefault();
      this.activeAddress = {};
      this.showAddressDialog_ = true;
    },

    /** @private */
    onAddressDialogClosed_: function() {
      this.showAddressDialog_ = false;
    },

    /**
     * Handles tapping on the "Edit" address button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onMenuEditAddressTap_: function(e) {
      e.preventDefault();
      this.showAddressDialog_ = true;
      this.$.addressSharedMenu.close();
    },

    /** @private */
    onRemoteEditAddressTap_: function() {
      window.open(this.i18n('manageAddressesUrl'));
    },

    /**
     * Handles tapping on the "Remove" address button.
     * @private
     */
    onMenuRemoveAddressTap_: function() {
      this.autofillManager_.removeAddress(
          /** @type {string} */(this.activeAddress.guid));
      this.$.addressSharedMenu.close();
    },

    /**
     * Opens the credit card action menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onCreditCardMenuTap_: function(e) {
      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);

      /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
       https://github.com/Polymer/polymer/issues/2574 */
      var item = menuEvent.model['dataHost']['dataHost'].item;

      // Copy item so dialog won't update model on cancel.
      this.activeCreditCard =
          /** @type {!chrome.autofillPrivate.CreditCardEntry} */(
              Object.assign({}, item));

      var dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
      /** @type {!CrActionMenuElement} */ (
          this.$.creditCardSharedMenu).showAt(dotsButton);
    },

    /**
     * Handles tapping on the "Add credit card" button.
     * @param {!Event} e
     * @private
     */
    onAddCreditCardTap_: function(e) {
      e.preventDefault();
      var date = new Date();  // Default to current month/year.
      var expirationMonth = date.getMonth() + 1;  // Months are 0 based.
      this.activeCreditCard = {
        expirationMonth: expirationMonth.toString(),
        expirationYear: date.getFullYear().toString(),
      };
      this.showCreditCardDialog_ = true;
    },

    /** @private */
    onCreditCardDialogClosed_: function() {
      this.showCreditCardDialog_ = false;
    },

    /**
     * Handles tapping on the "Edit" credit card button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onMenuEditCreditCardTap_: function(e) {
      e.preventDefault();
      this.showCreditCardDialog_ = true;
      this.$.creditCardSharedMenu.close();
    },

    /** @private */
    onRemoteEditCreditCardTap_: function() {
      window.open(this.i18n('manageCreditCardsUrl'));
    },

    /**
     * Handles tapping on the "Remove" credit card button.
     * @private
     */
    onMenuRemoveCreditCardTap_: function() {
      this.autofillManager_.removeCreditCard(
          /** @type {string} */(this.activeCreditCard.guid));
      this.$.creditCardSharedMenu.close();
    },

    /**
     * Handles tapping on the "Clear copy" button for cached credit cards.
     * @private
     */
    onMenuClearCreditCardTap_: function() {
      this.autofillManager_.clearCachedCreditCard(
          /** @type {string} */(this.activeCreditCard.guid));
      this.$.creditCardSharedMenu.close();
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
     * Listens for the save-address event, and calls the private API.
     * @param {!Event} event
     * @private
     */
    saveAddress_: function(event) {
      this.autofillManager_.saveAddress(event.detail);
    },

    /**
     * Listens for the save-credit-card event, and calls the private API.
     * @param {!Event} event
     * @private
     */
    saveCreditCard_: function(event) {
      this.autofillManager_.saveCreditCard(event.detail);
    },
  });
})();
