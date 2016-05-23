// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Autofill Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

// Fake data generator.
GEN_INCLUDE([ROOT_PATH +
    'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js']);

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
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
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
      var creditCard = FakeDataMaker.creditCardEntry();
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
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
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
      var address = FakeDataMaker.addressEntry();
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
