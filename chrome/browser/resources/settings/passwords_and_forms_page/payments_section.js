// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-section' is the section containing saved
 * credit cards for use in autofill and payments APIs.
 */

/**
 * Interface for all callbacks to the payments autofill API.
 * @interface
 */
class PaymentsManager {
  /**
   * Add an observer to the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  addCreditCardListChangedListener(listener) {}

  /**
   * Remove an observer from the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  removeCreditCardListChangedListener(listener) {}

  /**
   * Request the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} callback
   */
  getCreditCardList(callback) {}

  /** @param {string} guid The GUID of the credit card to remove.  */
  removeCreditCard(guid) {}

  /** @param {string} guid The GUID to credit card to remove from the cache. */
  clearCachedCreditCard(guid) {}

  /**
   * Saves the given credit card.
   * @param {!PaymentsManager.CreditCardEntry} creditCard
   */
  saveCreditCard(creditCard) {}
}

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
PaymentsManager.CreditCardEntry;

/**
 * Implementation that accesses the private API.
 * @implements {PaymentsManager}
 */
class PaymentsManagerImpl {
  /** @override */
  addCreditCardListChangedListener(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.addListener(listener);
  }

  /** @override */
  removeCreditCardListChangedListener(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.removeListener(listener);
  }

  /** @override */
  getCreditCardList(callback) {
    chrome.autofillPrivate.getCreditCardList(callback);
  }

  /** @override */
  removeCreditCard(guid) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  }

  /** @override */
  clearCachedCreditCard(guid) {
    chrome.autofillPrivate.maskCreditCard(assert(guid));
  }

  /** @override */
  saveCreditCard(creditCard) {
    chrome.autofillPrivate.saveCreditCard(creditCard);
  }
}

cr.addSingletonGetter(PaymentsManagerImpl);

(function() {
'use strict';

Polymer({
  is: 'settings-payments-section',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * An array of saved credit cards.
     * @type {!Array<!PaymentsManager.CreditCardEntry>}
     */
    creditCards: Array,

    /**
     * The model for any credit card related action menus or dialogs.
     * @private {?chrome.autofillPrivate.CreditCardEntry}
     */
    activeCreditCard: Object,

    /** @private */
    showCreditCardDialog_: Boolean,

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * TODO(sujiezhu): Use this to check migration requirements when all
     * information is ready. (https://crbug.com/852904).
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,
  },

  listeners: {
    'save-credit-card': 'saveCreditCard_',
  },

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /**
   * @type {PaymentsManager}
   * @private
   */
  PaymentsManager_: null,

  /**
   * @type {?function(!Array<!PaymentsManager.CreditCardEntry>)}
   * @private
   */
  setCreditCardsListener_: null,

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    // Create listener function.
    /** @type {function(!Array<!PaymentsManager.CreditCardEntry>)} */
    const setCreditCardsListener = list => {
      this.creditCards = list;
    };

    // Remember the bound reference in order to detach.
    this.setCreditCardsListener_ = setCreditCardsListener;

    // Set the managers. These can be overridden by tests.
    this.paymentsManager_ = PaymentsManagerImpl.getInstance();

    // Request initial data.
    this.paymentsManager_.getCreditCardList(setCreditCardsListener);

    // Listen for changes.
    this.paymentsManager_.addCreditCardListChangedListener(
        setCreditCardsListener);

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /** @override */
  detached: function() {
    this.paymentsManager_.removeCreditCardListChangedListener(
        /** @type {function(!Array<!PaymentsManager.CreditCardEntry>)} */ (
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
   * Opens the credit card action menu.
   * @param {!Event} e The polymer event.
   * @private
   */
  onCreditCardMenuTap_: function(e) {
    const menuEvent = /** @type {!{model: !{item: !Object}}} */ (e);

    /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
     https://github.com/Polymer/polymer/issues/2574 */
    // TODO(dpapad): The [dataHost][dataHost] workaround is only necessary for
    // Polymer 1. Remove once migration to Polymer 2 has completed.
    const item = Polymer.DomIf ? menuEvent.model.item :
                                 menuEvent.model['dataHost']['dataHost'].item;

    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard =
        /** @type {!chrome.autofillPrivate.CreditCardEntry} */ (
            Object.assign({}, item));

    const dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
    /** @type {!CrActionMenuElement} */ (this.$.creditCardSharedMenu)
        .showAt(dotsButton);
    this.activeDialogAnchor_ = dotsButton;
  },

  /**
   * Handles tapping on the "Add credit card" button.
   * @param {!Event} e
   * @private
   */
  onAddCreditCardTap_: function(e) {
    e.preventDefault();
    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    this.activeDialogAnchor_ = this.$.addCreditCard;
  },

  /** @private */
  onCreditCardDialogClose_: function() {
    this.showCreditCardDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Edit" credit card button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onMenuEditCreditCardTap_: function(e) {
    e.preventDefault();

    if (this.activeCreditCard.metadata.isLocal)
      this.showCreditCardDialog_ = true;
    else
      this.onRemoteEditCreditCardTap_();

    this.$.creditCardSharedMenu.close();
  },

  /** @private */
  onRemoteEditCreditCardTap_: function() {
    window.open(loadTimeData.getString('manageCreditCardsUrl'));
  },

  /**
   * Handles tapping on the "Remove" credit card button.
   * @private
   */
  onMenuRemoveCreditCardTap_: function() {
    this.paymentsManager_.removeCreditCard(
        /** @type {string} */ (this.activeCreditCard.guid));
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Clear copy" button for cached credit cards.
   * @private
   */
  onMenuClearCreditCardTap_: function() {
    this.paymentsManager_.clearCachedCreditCard(
        /** @type {string} */ (this.activeCreditCard.guid));
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard = null;
  },

  /**
   * The 3-dot menu should not be shown if the card is entirely remote.
   * @param {!chrome.autofillPrivate.AutofillMetadata} metadata
   * @return {boolean}
   * @private
   */
  showDots_: function(metadata) {
    return !!(metadata.isLocal || metadata.isCached);
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
   * Returns true if either pref value is false.
   * @param {boolean} pref1Value
   * @param {boolean} pref2Value
   * @return {boolean}
   * @private
   */
  eitherIsDisabled_: function(pref1Value, pref2Value) {
    return !pref1Value || !pref2Value;
  },

  /**
   * Listens for the save-credit-card event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  saveCreditCard_: function(event) {
    this.paymentsManager_.saveCreditCard(event.detail);
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },
});
})();
