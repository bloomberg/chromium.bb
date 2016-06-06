// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Autofill Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE([
    ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js',
    ROOT_PATH +
        'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js',
    ROOT_PATH + 'ui/webui/resources/js/load_time_data.js',
]);

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

    // Faking 'strings.js' for this test.
    loadTimeData.data = {
      editCreditCardTitle: 'edit-title',
      addCreditCardTitle: 'add-title'
    };
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
    var section = document.createElement('settings-autofill-section');
    section.addresses = addresses;
    section.creditCards = creditCards;
    document.body.appendChild(section);
    this.flushAutofillSection_(section);
    return section;
  },

  /**
   * Creates the Edit Credit Card dialog.
   * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
   * @return {!Object}
   */
  createCreditCardDialog_: function(creditCardItem) {
    var section = document.createElement('settings-credit-card-edit-dialog');
    document.body.appendChild(section);
    section.open(creditCardItem);  // Opening the dialog will add the item.
    Polymer.dom.flush();
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

    test('verifyAddVsEditCreditCardTitle', function() {
      var newCreditCard = FakeDataMaker.emptyCreditCardEntry();
      var newCreditCardDialog = self.createCreditCardDialog_(newCreditCard);
      var oldCreditCard = FakeDataMaker.creditCardEntry();
      var oldCreditCardDialog = self.createCreditCardDialog_(oldCreditCard);

      assertNotEquals(oldCreditCardDialog.title_, newCreditCardDialog.title_);
      assertNotEquals('', newCreditCardDialog.title_);
      assertNotEquals('', oldCreditCardDialog.title_);
    }),

    test('verifyExpiredCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // 2015 is over unless time goes wobbly.
      var twentyFifteen = 2015;
      creditCard.expirationYear = twentyFifteen.toString();

      var creditCardDialog = self.createCreditCardDialog_(creditCard);
      var selectableYears = creditCardDialog.$.yearList.items;
      var firstSelectableYear = selectableYears[0];
      var lastSelectableYear = selectableYears[selectableYears.length - 1];

      var now = new Date();
      var maxYear = now.getFullYear() + 9;

      assertEquals('2015', firstSelectableYear.textContent);
      assertEquals(maxYear.toString(), lastSelectableYear.textContent);
    }),

    test('verifyVeryFutureCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 20 years from now is unusual.
      var now = new Date();
      var farFutureYear = now.getFullYear() + 20;
      creditCard.expirationYear = farFutureYear.toString();

      var creditCardDialog = self.createCreditCardDialog_(creditCard);
      var selectableYears = creditCardDialog.$.yearList.items;
      var firstSelectableYear = selectableYears[0];
      var lastSelectableYear = selectableYears[selectableYears.length - 1];

      assertEquals(now.getFullYear().toString(),
          firstSelectableYear.textContent);
      assertEquals(farFutureYear.toString(), lastSelectableYear.textContent);
    }),

    test('verifyVeryNormalCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 2 years from now is not unusual.
      var now = new Date();
      var nearFutureYear = now.getFullYear() + 2;
      creditCard.expirationYear = nearFutureYear.toString();
      var maxYear = now.getFullYear() + 9;

      var creditCardDialog = self.createCreditCardDialog_(creditCard);
      var selectableYears = creditCardDialog.$.yearList.items;
      var firstSelectableYear = selectableYears[0];
      var lastSelectableYear = selectableYears[selectableYears.length - 1];

      assertEquals(now.getFullYear().toString(),
          firstSelectableYear.textContent);
      assertEquals(maxYear.toString(), lastSelectableYear.textContent);
    }),

    // Test will timeout if event is not received.
    test('verifySaveCreditCardEdit', function(done) {
      var creditCard = FakeDataMaker.emptyCreditCardEntry();
      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      creditCardDialog.addEventListener('save-credit-card', function(event) {
        assertEquals(creditCard.guid, event.detail.guid);
        done();
      });

      MockInteractions.tap(creditCardDialog.$.saveButton);
    }),

    test('verifyCancelCreditCardEdit', function(done) {
      var creditCard = FakeDataMaker.emptyCreditCardEntry();
      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      creditCardDialog.addEventListener('save-credit-card', function(event) {
        // Fail the test because the save event should not be called when cancel
        // is clicked.
        assertTrue(false);
        done();
      });

      creditCardDialog.addEventListener('iron-overlay-closed', function(event) {
        // Test is |done| in a timeout in order to ensure that
        // 'save-credit-card' is NOT fired after this test.
        window.setTimeout(done, 100);
      });

      MockInteractions.tap(creditCardDialog.$.cancelButton);
    }),

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
