// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses and credit cards for use in autofill.
 */
(function() {
  'use strict';

  Polymer({
    is: 'settings-autofill-section',

    properties: {
      /**
       * An array of saved addresses.
       * @type {!Array<!chrome.autofillPrivate.AddressEntry>}
       */
      addresses: {
        type: Array,
      },

      /**
       * An array of saved addresses.
       * @type {!Array<!chrome.autofillPrivate.CreditCardEntry>}
       */
      creditCards: {
        type: Array,
      },
    },

    /**
     * Formats an AddressEntry so it's displayed as an address.
     * @param {!chrome.autofillPrivate.AddressEntry} item
     * @return {!string}
     */
    address_: function(item) {
      return item.metadata.summaryLabel + item.metadata.summarySublabel;
    },

    /**
     * Formats the expiration date so it's displayed as MM/YYYY.
     * @param {!chrome.autofillPrivate.CreditCardEntry} item
     * @return {!string}
     */
    expiration_: function(item) {
      return item.expirationMonth + '/' + item.expirationYear;
    },

    /**
     * Handles tapping on the "Add address" button.
     * @param {!Event} e
     */
    onAddAddressTap_: function(e) {
      // TODO(hcarmona): implement this.
      e.preventDefault();
    },

    /**
     * Handles tapping on the "Add credit card" button.
     * @param {!Event} e
     */
    onAddCreditCardTap_: function(e) {
      // TODO(hcarmona): implement this.
      e.preventDefault();
    },
  });
})();
