// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Autofill Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js',
]);

/**
 * Test implementation.
 * @implements {settings.address.CountryDetailManager}
 * @constructor
 */
function CountryDetailManagerTestImpl() {}

CountryDetailManagerTestImpl.prototype = {
  /** @override */
  getCountryList: function() {
    return new Promise(function(resolve) {
      resolve([
        {name: 'United States', countryCode: 'US'},  // Default test country.
        {name: 'Israel', countryCode: 'IL'},
        {name: 'United Kingdom', countryCode: 'GB'},
      ]);
    });
  },

  /** @override */
  getAddressFormat: function(countryCode) {
    return new Promise(function(resolve) {
      chrome.autofillPrivate.getAddressComponents(countryCode, resolve);
    });
  },
};

/**
 * Will call |loopBody| for each item in |items|. Will only move to the next
 * item after the promise from |loopBody| resolves.
 * @param {!Array<Object>} items
 * @param {!function(!Object):!Promise} loopBody
 * @return {!Promise}
 */
function asyncForEach(items, loopBody) {
  return new Promise(function(resolve) {
    var index = 0;

    function loop() {
      var item = items[index++];
      if (item)
        loopBody(item).then(loop);
      else
        resolve();
    };

    loop();
  });
}

/**
 * Resolves the promise after the element fires the expected event. Will add and
 * remove the listener so it is only triggered once. |causeEvent| is called
 * after adding a listener to make sure that the event is captured.
 * @param {!Element} element
 * @param {string} eventName
 * @param {function():void} causeEvent
 * @return {!Promise}
 */
function expectEvent(element, eventName, causeEvent) {
  return new Promise(function(resolve) {
    var callback = function() {
      element.removeEventListener(eventName, callback);
      resolve.apply(this, arguments);
    };
    element.addEventListener(eventName, callback);
    causeEvent();
  });
}

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
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'passwords_and_autofill_fake_data.js',
    'test_util.js',
    'ensure_lazy_loaded.js',
  ]),

  /**
   * TODO(hcarmona): Increases speed, but disables A11y checks. Enable checks
   * when we "accessibilityIssuesAreErrors: true" for all tests.
   * @override
   */
  runAccessibilityChecks: false,

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');

    settings.ensureLazyLoaded();
  },

  /**
   * Creates the autofill section for the given lists.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @return {!Object}
   * @private
   */
  createAutofillSection_: function(addresses, creditCards) {
    // Override the AutofillManagerImpl for testing.
    this.autofillManager = new TestAutofillManager();
    this.autofillManager.data.addresses = addresses;
    this.autofillManager.data.creditCards = creditCards;
    AutofillManagerImpl.instance_ = this.autofillManager;

    var section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    Polymer.dom.flush();
    return section;
  },

  /**
   * Creates the Edit Address dialog and fulfills the promise when the dialog
   * has actually opened.
   * @param {!chrome.autofillPrivate.AddressEntry} address
   * @return {!Promise<Object>}
   */
  createAddressDialog_: function(address) {
    return new Promise(function(resolve) {
      var section = document.createElement('settings-address-edit-dialog');
      section.address = address;
      document.body.appendChild(section);
      section.addEventListener('on-update-address-wrapper', function() {
        resolve(section);
      });
    });
  },

  /**
   * Creates the Edit Credit Card dialog.
   * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
   * @return {!Object}
   */
  createCreditCardDialog_: function(creditCardItem) {
    var section = document.createElement('settings-credit-card-edit-dialog');
    section.creditCard = creditCardItem;
    document.body.appendChild(section);
    Polymer.dom.flush();
    return section;
  },
};

TEST_F('SettingsAutofillSectionBrowserTest', 'uiTest', function() {
  suite('AutofillSection', function() {
    test('testAutofillExtensionIndicator', function() {
      // Initializing with fake prefs
      var section = document.createElement('settings-autofill-section');
      section.prefs = {autofill: {enabled: {}}};
      document.body.appendChild(section);

      assertFalse(!!section.$$('#autofillExtensionIndicator'));
      section.set('prefs.autofill.enabled.extensionId', 'test-id');
      Polymer.dom.flush();

      assertTrue(!!section.$$('#autofillExtensionIndicator'));
    });
  });

  mocha.run();
});

TEST_F('SettingsAutofillSectionBrowserTest', 'CreditCardTests', function() {
  var self = this;

  suite('AutofillSection', function() {
    suiteSetup(function() {
      settings.address.CountryDetailManagerImpl.instance_ =
          new CountryDetailManagerTestImpl();
    });

    setup(function() {
      PolymerTest.clearBody();
    });

    test('verifyCreditCardCount', function() {
      var section = self.createAutofillSection_([], []);
      assertTrue(!!section);

      var creditCardList = section.$.creditCardList;
      assertTrue(!!creditCardList);
      // +1 for the template element.
      assertEquals(1, creditCardList.children.length);

      assertFalse(section.$.noCreditCardsLabel.hidden);
      assertTrue(section.$.creditCardsHeading.hidden);
    });

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
      // +1 for the template element.
      assertEquals(creditCards.length + 1, creditCardList.children.length);

      assertTrue(section.$.noCreditCardsLabel.hidden);
      assertFalse(section.$.creditCardsHeading.hidden);
    });

    test('verifyCreditCardFields', function() {
      var creditCard = FakeDataMaker.creditCardEntry();
      var section = self.createAutofillSection_([], [creditCard]);
      var creditCardList = section.$.creditCardList;
      var row = creditCardList.children[0];
      assertTrue(!!row);

      assertEquals(creditCard.metadata.summaryLabel,
                   row.querySelector('#creditCardLabel').textContent);
      assertEquals(creditCard.expirationMonth + '/' + creditCard.expirationYear,
                   row.querySelector('#creditCardExpiration').textContent);
    });

    test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
      var creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = true;
      var section = self.createAutofillSection_([], [creditCard]);
      var creditCardList = section.$.creditCardList;
      var row = creditCardList.children[0];
      assertTrue(!!row);
      var menuButton = row.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      var outlinkButton = row.querySelector('button.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
      var creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = false;
      var section = self.createAutofillSection_([], [creditCard]);
      var creditCardList = section.$.creditCardList;
      var row = creditCardList.children[0];
      assertTrue(!!row);
      var menuButton = row.querySelector('#creditCardMenu');
      assertFalse(!!menuButton);
      var outlinkButton = row.querySelector('button.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddVsEditCreditCardTitle', function() {
      var newCreditCard = FakeDataMaker.emptyCreditCardEntry();
      var newCreditCardDialog = self.createCreditCardDialog_(newCreditCard);
      var oldCreditCard = FakeDataMaker.creditCardEntry();
      var oldCreditCardDialog = self.createCreditCardDialog_(oldCreditCard);

      assertNotEquals(oldCreditCardDialog.title_, newCreditCardDialog.title_);
      assertNotEquals('', newCreditCardDialog.title_);
      assertNotEquals('', oldCreditCardDialog.title_);

      // Wait for dialogs to open before finishing test.
      return Promise.all([
        test_util.whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
        test_util.whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
      ]);
    });

    test('verifyExpiredCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // 2015 is over unless time goes wobbly.
      var twentyFifteen = 2015;
      creditCard.expirationYear = twentyFifteen.toString();

      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        var now = new Date();
        var maxYear = now.getFullYear() + 9;
        var yearOptions = creditCardDialog.$.year.options;

        assertEquals('2015', yearOptions[0].textContent.trim());
        assertEquals(
            maxYear.toString(),
            yearOptions[yearOptions.length -1].textContent.trim());
        assertEquals(
            creditCard.expirationYear, creditCardDialog.$.year.value);
      });
    });

    test('verifyVeryFutureCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 20 years from now is unusual.
      var now = new Date();
      var farFutureYear = now.getFullYear() + 20;
      creditCard.expirationYear = farFutureYear.toString();

      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        var yearOptions = creditCardDialog.$.year.options;

        assertEquals(
            now.getFullYear().toString(),
            yearOptions[0].textContent.trim());
        assertEquals(
            farFutureYear.toString(),
            yearOptions[yearOptions.length -1].textContent.trim());
        assertEquals(
            creditCard.expirationYear, creditCardDialog.$.year.value);
      });
    });

    test('verifyVeryNormalCreditCardYear', function() {
      var creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 2 years from now is not unusual.
      var now = new Date();
      var nearFutureYear = now.getFullYear() + 2;
      creditCard.expirationYear = nearFutureYear.toString();
      var maxYear = now.getFullYear() + 9;

      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        var yearOptions = creditCardDialog.$.year.options;

        assertEquals(
            now.getFullYear().toString(),
            yearOptions[0].textContent.trim());
        assertEquals(
            maxYear.toString(),
            yearOptions[yearOptions.length -1].textContent.trim());
        assertEquals(
            creditCard.expirationYear, creditCardDialog.$.year.value);
      });
    });

    test('verify save disabled for expired credit card', function() {
      var creditCard = FakeDataMaker.emptyCreditCardEntry();

      var now = new Date();
      creditCard.expirationYear = now.getFullYear() - 2;
      // works fine for January.
      creditCard.expirationMonth = now.getMonth() - 1;

      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        assertTrue(creditCardDialog.$.saveButton.disabled);
      });
    });

    // Test will timeout if event is not received.
    test('verify save new credit card', function(done) {
      var creditCard = FakeDataMaker.emptyCreditCardEntry();
      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        // Not expired, but still can't be saved, because there's no name.
        assertTrue(creditCardDialog.$.expired.hidden);
        assertTrue(creditCardDialog.$.saveButton.disabled);

        // Add a name and trigger the on-input handler.
        creditCardDialog.set('creditCard.name', 'Jane Doe');
        creditCardDialog.onCreditCardNameOrNumberChanged_();
        Polymer.dom.flush();

        assertTrue(creditCardDialog.$.expired.hidden);
        assertFalse(creditCardDialog.$.saveButton.disabled);

        creditCardDialog.addEventListener('save-credit-card', function(event) {
          assertEquals(creditCard.guid, event.detail.guid);
          done();
        });
        MockInteractions.tap(creditCardDialog.$.saveButton);
      });
    });

    test('verifyCancelCreditCardEdit', function(done) {
      var creditCard = FakeDataMaker.emptyCreditCardEntry();
      var creditCardDialog = self.createCreditCardDialog_(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').then(function() {
        creditCardDialog.addEventListener('save-credit-card', function() {
          // Fail the test because the save event should not be called when
          // cancel is clicked.
          assertTrue(false);
          done();
        });

        creditCardDialog.addEventListener('close', function() {
          // Test is |done| in a timeout in order to ensure that
          // 'save-credit-card' is NOT fired after this test.
          window.setTimeout(done, 100);
        });

        MockInteractions.tap(creditCardDialog.$.cancelButton);
      });
    });
  });

  mocha.run();
});

TEST_F('SettingsAutofillSectionBrowserTest', 'AddressTests', function() {
  var self = this;


  suite('AutofillSection', function() {
    suiteSetup(function() {
      settings.address.CountryDetailManagerImpl.instance_ =
          new CountryDetailManagerTestImpl();
    });

    setup(function() {
      PolymerTest.clearBody();
    });

    test('verifyNoAddresses', function() {
      var section = self.createAutofillSection_([], []);
      assertTrue(!!section);

      var addressList = section.$.addressList;
      assertTrue(!!addressList);
      // 1 for the template element.
      assertEquals(1, addressList.children.length);

      assertFalse(section.$.noAddressesLabel.hidden);
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
      // +1 for the template element.
      assertEquals(addresses.length + 1, addressList.children.length);

      assertTrue(section.$.noAddressesLabel.hidden);
    });

    test('verifyAddressFields', function() {
      var address = FakeDataMaker.addressEntry();
      var section = self.createAutofillSection_([address], []);
      var addressList = section.$.addressList;
      var row = addressList.children[0];
      assertTrue(!!row);

      var addressSummary = address.metadata.summaryLabel +
                           address.metadata.summarySublabel;

      var actualSummary = '';

      // Eliminate white space between nodes!
      var addressPieces = row.querySelector('#addressSummary').children;
      for (var i = 0; i < addressPieces.length; ++i) {
        actualSummary += addressPieces[i].textContent.trim();
      }

      assertEquals(addressSummary, actualSummary);
    });

    test('verifyAddressRowButtonIsDropdownWhenLocal', function() {
      var address = FakeDataMaker.addressEntry();
      address.metadata.isLocal = true;
      var section = self.createAutofillSection_([address], []);
      var addressList = section.$.addressList;
      var row = addressList.children[0];
      assertTrue(!!row);
      var menuButton = row.querySelector('#addressMenu')
      assertTrue(!!menuButton);
      var outlinkButton = row.querySelector('button.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyAddressRowButtonIsOutlinkWhenRemote', function() {
      var address = FakeDataMaker.addressEntry();
      address.metadata.isLocal = false;
      var section = self.createAutofillSection_([address], []);
      var addressList = section.$.addressList;
      var row = addressList.children[0];
      assertTrue(!!row);
      var menuButton = row.querySelector('#addressMenu')
      assertFalse(!!menuButton);
      var outlinkButton = row.querySelector('button.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddAddressDialog', function() {
      return self.createAddressDialog_(
          FakeDataMaker.emptyAddressEntry()).then(function(dialog) {
        var title = dialog.$$('.title');
        assertEquals(loadTimeData.getString('addAddressTitle'),
            title.textContent);
        // Shouldn't be possible to save until something is typed in.
        assertTrue(dialog.$.saveButton.disabled);
      });
    });

    test('verifyEditAddressDialog', function() {
      return self.createAddressDialog_(
          FakeDataMaker.addressEntry()).then(function(dialog) {
        var title = dialog.$$('.title');
        assertEquals(
            loadTimeData.getString('editAddressTitle'), title.textContent);
        // Should be possible to save when editing because fields are populated.
        assertFalse(dialog.$.saveButton.disabled);
      });
    });

    test('verifyCountryIsSaved', function() {
      var address = FakeDataMaker.emptyAddressEntry();
      return self.createAddressDialog_(address).then(function(dialog) {
        var countrySelect = dialog.$$('select');
        assertEquals('', countrySelect.value);
        assertEquals(undefined, address.countryCode);
        countrySelect.value = 'US';
        countrySelect.dispatchEvent(new CustomEvent('change'));
        Polymer.dom.flush();
        assertEquals('US', countrySelect.value);
        assertEquals('US', address.countryCode);
      });
    });

    test('verifyPhoneAndEmailAreSaved', function() {
      var address = FakeDataMaker.emptyAddressEntry();
      return self.createAddressDialog_(address).then(function(dialog) {
        assertEquals('', dialog.$.phoneInput.value);
        assertFalse(!!(address.phoneNumbers && address.phoneNumbers[0]));

        assertEquals('', dialog.$.emailInput.value);
        assertFalse(!!(address.emailAddresses && address.emailAddresses[0]));

        var phoneNumber = '(555) 555-5555';
        var emailAddress = 'no-reply@chromium.org';

        dialog.$.phoneInput.value = phoneNumber;
        dialog.$.emailInput.value = emailAddress;

        return expectEvent(dialog, 'save-address', function() {
          MockInteractions.tap(dialog.$.saveButton);
        }).then(function() {
          assertEquals(phoneNumber, dialog.$.phoneInput.value);
          assertEquals(phoneNumber, address.phoneNumbers[0]);

          assertEquals(emailAddress, dialog.$.emailInput.value);
          assertEquals(emailAddress, address.emailAddresses[0]);
        });
      });
    });

    test('verifyPhoneAndEmailAreRemoved', function() {
      var address = FakeDataMaker.emptyAddressEntry();

      var phoneNumber = '(555) 555-5555';
      var emailAddress = 'no-reply@chromium.org';

      address.countryCode = 'US';  // Set to allow save to be active.
      address.phoneNumbers = [phoneNumber];
      address.emailAddresses = [emailAddress];

      return self.createAddressDialog_(address).then(function(dialog) {
        assertEquals(phoneNumber, dialog.$.phoneInput.value);
        assertEquals(emailAddress, dialog.$.emailInput.value);

        dialog.$.phoneInput.value = '';
        dialog.$.emailInput.value = '';

        return expectEvent(dialog, 'save-address', function() {
          MockInteractions.tap(dialog.$.saveButton);
        }).then(function() {
          assertEquals(0, address.phoneNumbers.length);
          assertEquals(0, address.emailAddresses.length);
        });
      });
    });

    // Test will set a value of 'foo' in each text field and verify that the
    // save button is enabled, then it will clear the field and verify that the
    // save button is disabled. Test passes after all elements have been tested.
    test('verifySaveIsNotClickableIfAllInputFieldsAreEmpty', function() {
      return self.createAddressDialog_(
          FakeDataMaker.emptyAddressEntry()).then(function(dialog) {
        var saveButton = dialog.$.saveButton;
        var testElements =
            dialog.$.dialog.querySelectorAll('paper-input,paper-textarea');

        // Default country is 'US' expecting: Name, Organization,
        // Street address, City, State, ZIP code, Phone, and Email.
        assertEquals(8, testElements.length);

        return asyncForEach(testElements, function(element) {
          return expectEvent(dialog, 'on-update-can-save', function() {
            assertTrue(saveButton.disabled);
            element.value = 'foo';
          }).then(function() {
            return expectEvent(dialog, 'on-update-can-save', function() {
              assertFalse(saveButton.disabled);
              element.value = '';
            });
          }).then(function() {
            assertTrue(saveButton.disabled);
          });
        });
      });
    });

    // Setting the country should allow the address to be saved.
    test('verifySaveIsNotClickableIfCountryNotSet', function() {
      var dialog = null;

      var simulateCountryChange = function(countryCode) {
        var countrySelect = dialog.$$('select');
        countrySelect.value = countryCode;
        countrySelect.dispatchEvent(new CustomEvent('change'));
      };

      return self.createAddressDialog_(
          FakeDataMaker.emptyAddressEntry()).then(function(d) {
        dialog = d;
        assertTrue(dialog.$.saveButton.disabled);

        return expectEvent(
            dialog, 'on-update-can-save',
            simulateCountryChange.bind(null, 'US'));
      }).then(function() {
        assertFalse(dialog.$.saveButton.disabled);

        return expectEvent(
            dialog, 'on-update-can-save',
            simulateCountryChange.bind(null, ''));
      }).then(function() {
        assertTrue(dialog.$.saveButton.disabled);
      });
    });

    // Test will timeout if save-address event is not fired.
    test('verifyDefaultCountryIsAppliedWhenSaving', function() {
      var address = FakeDataMaker.emptyAddressEntry();
      address.companyName = 'Google';
      return self.createAddressDialog_(address).then(function(dialog) {
        return expectEvent(dialog, 'save-address', function() {
          // Verify |countryCode| is not set.
          assertEquals(undefined, address.countryCode);
          MockInteractions.tap(dialog.$.saveButton);
        }).then(function(event) {
          // 'US' is the default country for these tests.
          assertEquals('US', event.detail.countryCode);
        });
      });
    });

    test('verifyCancelDoesNotSaveAddress', function(done) {
      self.createAddressDialog_(
          FakeDataMaker.addressEntry()).then(function(dialog) {
        dialog.addEventListener('save-address', function() {
          // Fail the test because the save event should not be called when
          // cancel is clicked.
          assertTrue(false);
        });

        dialog.addEventListener('close', function() {
          // Test is |done| in a timeout in order to ensure that
          // 'save-address' is NOT fired after this test.
          window.setTimeout(done, 100);
        });

        MockInteractions.tap(dialog.$.cancelButton);
      });
    });
  });

  mocha.run();
});

TEST_F('SettingsAutofillSectionBrowserTest', 'AddressLocaleTests', function() {
  var self = this;

  suite('AutofillSection', function() {
    suiteSetup(function() {
      settings.address.CountryDetailManagerImpl.instance_ =
          new CountryDetailManagerTestImpl();
    });

    setup(function() {
      PolymerTest.clearBody();
    });

    // US address has 3 fields on the same line.
    test('verifyEditingUSAddress', function() {
      var address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = [ 'Name' ];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'City';
      address.addressLevel1 = 'State';
      address.postalCode = 'ZIP code';
      address.countryCode = 'US';
      address.phoneNumbers = [ 'Phone' ];
      address.emailAddresses = [ 'Email' ];

      return self.createAddressDialog_(address).then(function(dialog) {
        var rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(6, rows.length);

        // Name
        var row = rows[0];
        var cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.fullNames[0], cols[0].value);
        // Organization
        row = rows[1];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        // Street address
        row = rows[2];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.addressLines, cols[0].value);
        // City, State, ZIP code
        row = rows[3];
        cols = row.querySelectorAll('.address-column');
        assertEquals(3, cols.length);
        assertEquals(address.addressLevel2, cols[0].value);
        assertEquals(address.addressLevel1, cols[1].value);
        assertEquals(address.postalCode, cols[2].value);
        // Country
        row = rows[4];
        var countrySelect = row.querySelector('select');
        assertTrue(!!countrySelect);
        assertEquals(
            'United States',
            countrySelect.selectedOptions[0].textContent.trim());
        // Phone, Email
        row = rows[5];
        cols = row.querySelectorAll('.address-column');
        assertEquals(2, cols.length);
        assertEquals(address.phoneNumbers[0], cols[0].value);
        assertEquals(address.emailAddresses[0], cols[1].value);
      });
    });

    // GB address has 1 field per line for all lines that change.
    test('verifyEditingGBAddress', function() {
      var address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = [ 'Name' ];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'Post town';
      address.postalCode = 'Postal code';
      address.countryCode = 'GB';
      address.phoneNumbers = [ 'Phone' ];
      address.emailAddresses = [ 'Email' ];

      return self.createAddressDialog_(address).then(function(dialog) {
        var rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(7, rows.length);

        // Name
        var row = rows[0];
        var cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.fullNames[0], cols[0].value);
        // Organization
        row = rows[1];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        // Street address
        row = rows[2];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.addressLines, cols[0].value);
        // Post Town
        row = rows[3];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.addressLevel2, cols[0].value);
        // Postal code
        row = rows[4];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.postalCode, cols[0].value);
        // Country
        row = rows[5];
        var countrySelect = row.querySelector('select');
        assertTrue(!!countrySelect);
        assertEquals(
            'United Kingdom',
            countrySelect.selectedOptions[0].textContent.trim());
        // Phone, Email
        row = rows[6];
        cols = row.querySelectorAll('.address-column');
        assertEquals(2, cols.length);
        assertEquals(address.phoneNumbers[0], cols[0].value);
        assertEquals(address.emailAddresses[0], cols[1].value);
      });
    });

    // IL address has 2 fields on the same line and is an RTL locale.
    // RTL locale shouldn't affect this test.
    test('verifyEditingILAddress', function() {
      var address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = [ 'Name' ];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'City';
      address.postalCode = 'Postal code';
      address.countryCode = 'IL';
      address.phoneNumbers = [ 'Phone' ];
      address.emailAddresses = [ 'Email' ];

      return self.createAddressDialog_(address).then(function(dialog) {
        var rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(6, rows.length);

        // Name
        var row = rows[0];
        var cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.fullNames[0], cols[0].value);
        // Organization
        row = rows[1];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        // Street address
        row = rows[2];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.addressLines, cols[0].value);
        // City, Postal code
        row = rows[3];
        cols = row.querySelectorAll('.address-column');
        assertEquals(2, cols.length);
        assertEquals(address.addressLevel2, cols[0].value);
        assertEquals(address.postalCode, cols[1].value);
        // Country
        row = rows[4];
        var countrySelect = row.querySelector('select');
        assertTrue(!!countrySelect);
        assertEquals(
            'Israel', countrySelect.selectedOptions[0].textContent.trim());
        // Phone, Email
        row = rows[5];
        cols = row.querySelectorAll('.address-column');
        assertEquals(2, cols.length);
        assertEquals(address.phoneNumbers[0], cols[0].value);
        assertEquals(address.emailAddresses[0], cols[1].value);
      });
    });

    // US has an extra field 'State'. Validate that this field is
    // persisted when switching to IL then back to US.
    test('verifyAddressPersistanceWhenSwitchingCountries', function() {
      var address = FakeDataMaker.emptyAddressEntry();
      address.countryCode = 'US';

      return self.createAddressDialog_(address).then(function(dialog) {
        var city = 'Los Angeles';
        var state = 'CA';
        var zip = '90291';
        var countrySelect = dialog.$$('select');

        return expectEvent(dialog, 'on-update-address-wrapper', function() {
          // US:
          var rows = dialog.$.dialog.querySelectorAll('.address-row');
          assertEquals(6, rows.length);

          // City, State, ZIP code
          var row = rows[3];
          var cols = row.querySelectorAll('.address-column');
          assertEquals(3, cols.length);
          cols[0].value = city;
          cols[1].value = state;
          cols[2].value = zip;

          countrySelect.value = 'IL';
          countrySelect.dispatchEvent(new CustomEvent('change'));
        }).then(function() {
          return expectEvent(dialog, 'on-update-address-wrapper', function() {
            // IL:
            rows = dialog.$.dialog.querySelectorAll('.address-row');
            assertEquals(6, rows.length);

            // City, Postal code
            row = rows[3];
            cols = row.querySelectorAll('.address-column');
            assertEquals(2, cols.length);
            assertEquals(city, cols[0].value);
            assertEquals(zip, cols[1].value);

            countrySelect.value = 'US';
            countrySelect.dispatchEvent(new CustomEvent('change'));
          });
        }).then(function() {
          // US:
          var rows = dialog.$.dialog.querySelectorAll('.address-row');
          assertEquals(6, rows.length);

          // City, State, ZIP code
          row = rows[3];
          cols = row.querySelectorAll('.address-column');
          assertEquals(3, cols.length);
          assertEquals(city, cols[0].value);
          assertEquals(state, cols[1].value);
          assertEquals(zip, cols[2].value);
        });
      });
    });
  });

  mocha.run();
});
