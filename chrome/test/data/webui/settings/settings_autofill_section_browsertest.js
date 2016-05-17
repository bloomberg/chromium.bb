// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Autofill Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsAutofillSectionBrowserTest() {}

SettingsAutofillSectionBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/passwords_and_forms_page/autofill_section.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');
  },

  /**
   * Replaces any 'x' in a string with a random number of the base.
   * @param {!string} pattern The pattern that should be used as an input.
   * @param {!number} base The number base. ie: 16 for hex or 10 for decimal.
   * @return {!string}
   * @private
   */
  patternMaker_: function(pattern, base) {
    return pattern.replace(/x/g, function() {
      return Math.floor(Math.random() * base).toString(base);
    });
  },

  /**
   * Creates a new random GUID for testing.
   * @return {!string}
   * @private
   */
  makeGuid_: function() {
    return this.patternMaker_('xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
  },

  /**
   * Creates a fake address entry for testing.
   * @return {!chrome.autofillPrivate.AddressEntry}
   * @private
   */
  createFakeAddressEntry_: function() {
    var ret = {};
    ret.guid = this.makeGuid_();
    ret.fullNames = ['John', 'Doe'];
    ret.companyName = 'Google';
    ret.addressLines = this.patternMaker_('xxxx Main St', 10);
    ret.addressLevel1 = "CA";
    ret.addressLevel2 = "Venice";
    ret.postalCode = this.patternMaker_('xxxxx', 10);
    ret.countryCode = 'US';
    ret.phoneNumbers = [this.patternMaker_('(xxx) xxx-xxxx', 10)];
    ret.emailAddresses = [this.patternMaker_('userxxxx@gmail.com', 16)];
    ret.languageCode = 'EN-US';
    ret.metadata = {isLocal: true};
    ret.metadata.summaryLabel = ret.fullNames[0];
    ret.metadata.summarySublabel = ' ' + ret.addressLines;
    return ret;
  },

  /**
   * Creates a new random credit card entry for testing.
   * @return {!chrome.autofillPrivate.CreditCardEntry}
   * @private
   */
  createFakeCreditCardEntry_: function() {
    var ret = {};
    ret.guid = this.makeGuid_();
    ret.name = 'Jane Doe';
    ret.cardNumber = this.patternMaker_('xxxx xxxx xxxx xxxx', 10);
    ret.expirationMonth = Math.ceil(Math.random() * 11).toString();
    ret.expirationYear = (2016 + Math.floor(Math.random() * 5)).toString();
    ret.metadata = {isLocal: true};
    var cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
    var card = cards[Math.floor(Math.random() * cards.length)];
    ret.metadata.summaryLabel = card + ' ' + '****' + ret.cardNumber.substr(-4);
    return ret;
  },

  /**
   * Allow the iron-list to be sized properly.
   * @param {!Object} autofillSection
   * @private
   */
  flushAutofillSection_: function(autofillSection) {
    autofillSection.$.addressList.notifyResize();
    autofillSection.$.creditCardList.notifyResize();
    Polymer.dom.flush();
  },
  /**
   * Creates the autofill section for the given lists.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList
   * @return {!Object}
   * @private
   */
  createAutofillSection_: function(addresses, creditCards) {
    // Create a passwords-section to use for testing.
    var section = document.createElement('settings-autofill-section');
    section.addresses = addresses;
    section.creditCards = creditCards;
    document.body.appendChild(section);
    this.flushAutofillSection_(section);
    return section;
  },
};

/**
 * This test will validate that the section is loaded with data.
 */
TEST_F('SettingsAutofillSectionBrowserTest', 'uiTests', function() {
  var self = this;

  suite('AutofillSection', function() {
    test('verifyCreditCardCount', function() {
      var creditCards = [
        self.createFakeCreditCardEntry_(),
        self.createFakeCreditCardEntry_(),
        self.createFakeCreditCardEntry_(),
        self.createFakeCreditCardEntry_(),
        self.createFakeCreditCardEntry_(),
        self.createFakeCreditCardEntry_(),
      ];

      var section = self.createAutofillSection_([], creditCards);

      assertTrue(!!section);
      var creditCardList = section.$.creditCardList;
      assertTrue(!!creditCardList);
      assertEquals(creditCards, creditCardList.items);
      // +1 for the template element.
      assertEquals(creditCards.length + 1, creditCardList.children.length);
    });

    test('verifyCreditCardFields', function() {
      var creditCard = self.createFakeCreditCardEntry_();
      var section = self.createAutofillSection_([], [creditCard]);
      var creditCardList = section.$.creditCardList;
      var row = creditCardList.children[1];  // Skip over the template.
      assertTrue(!!row);

      assertEquals(creditCard.metadata.summaryLabel,
                   row.querySelector('#creditCardLabel').textContent);
      assertEquals(creditCard.expirationMonth + '/' + creditCard.expirationYear,
                   row.querySelector('#creditCardExpiration').textContent);
    });

    test('verifyAddressCount', function() {
      var addresses = [
        self.createFakeAddressEntry_(),
        self.createFakeAddressEntry_(),
        self.createFakeAddressEntry_(),
        self.createFakeAddressEntry_(),
        self.createFakeAddressEntry_(),
      ];

      var section = self.createAutofillSection_(addresses, []);

      assertTrue(!!section);
      var addressList = section.$.addressList;
      assertTrue(!!addressList);
      assertEquals(addresses, addressList.items);
      // +1 for the template element.
      assertEquals(addresses.length + 1, addressList.children.length);
    });

    test('verifyAddressFields', function() {
      var address = self.createFakeAddressEntry_();
      var section = self.createAutofillSection_([address], []);
      var addressList = section.$.addressList;
      var row = addressList.children[1];  // Skip over the template.
      assertTrue(!!row);

      var addressSummary = address.metadata.summaryLabel +
                           address.metadata.summarySublabel;

      assertEquals(addressSummary,
                   row.querySelector('#addressSummary').textContent);
    });
  });

  mocha.run();
});
