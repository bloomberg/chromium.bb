// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_autofill_section', function() {
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
   * Resolves the promise after the element fires the expected event. Will add
   * and remove the listener so it is only triggered once. |causeEvent| is
   * called after adding a listener to make sure that the event is captured.
   * @param {!Element} element
   * @param {string} eventName
   * @param {function():void} causeEvent
   * @return {!Promise}
   */
  function expectEvent(element, eventName, causeEvent) {
    return new Promise(function(resolve) {
      const callback = function() {
        element.removeEventListener(eventName, callback);
        resolve.apply(this, arguments);
      };
      element.addEventListener(eventName, callback);
      causeEvent();
    });
  }

  /**
   * Creates the autofill section for the given list.
   * @param {!Array<!chrome.autofillPrivate.AddressEntry>} addresses
   * @param {!Object} prefValues
   * @return {!Object}
   */
  function createAutofillSection(addresses, prefValues) {
    // Override the AutofillManagerImpl for testing.
    this.autofillManager = new TestAutofillManager();
    this.autofillManager.data.addresses = addresses;
    AutofillManagerImpl.instance_ = this.autofillManager;

    const section = document.createElement('settings-autofill-section');
    section.prefs = {autofill: prefValues};
    document.body.appendChild(section);
    Polymer.dom.flush();

    return section;
  }

  /**
   * Creates the Edit Address dialog and fulfills the promise when the dialog
   * has actually opened.
   * @param {!chrome.autofillPrivate.AddressEntry} address
   * @return {!Promise<Object>}
   */
  function createAddressDialog(address) {
    return new Promise(function(resolve) {
      const section = document.createElement('settings-address-edit-dialog');
      section.address = address;
      document.body.appendChild(section);
      test_util.eventToPromise('on-update-address-wrapper', section)
        .then(function() { resolve(section); });
    });
  }

  suite('AutofillSectionUiTest', function() {
    test('testAutofillExtensionIndicator', function() {
      // Initializing with fake prefs
      const section = document.createElement('settings-autofill-section');
      section.prefs = {
        autofill: {enabled: {}, profile_enabled: {}}
      };
      document.body.appendChild(section);

      assertFalse(!!section.$$('#autofillExtensionIndicator'));
      section.set('prefs.autofill.profile_enabled.extensionId', 'test-id');
      Polymer.dom.flush();

      assertTrue(!!section.$$('#autofillExtensionIndicator'));
    });
  });

  suite('AutofillSectionAddressTests', function() {
    suiteSetup(function() {
      settings.address.CountryDetailManagerImpl.instance_ =
          new CountryDetailManagerTestImpl();
    });

    setup(function() {
      PolymerTest.clearBody();
    });

    /**
     * Will call |loopBody| for each item in |items|. Will only move to the next
     * item after the promise from |loopBody| resolves.
     * @param {!Array<Object>} items
     * @param {!function(!Object):!Promise} loopBody
     * @return {!Promise}
     */
    function asyncForEach(items, loopBody) {
      return new Promise(function(resolve) {
        let index = 0;

        function loop() {
          const item = items[index++];
          if (item)
            loopBody(item).then(loop);
          else
            resolve();
        }

        loop();
      });
    }

    test('verifyNoAddresses', function() {
      const section = createAutofillSection(
          [], {enabled: {value: true}, profile_enabled: {value: true}});

      const addressList = section.$.addressList;
      assertTrue(!!addressList);
      // 1 for the template element.
      assertEquals(1, addressList.children.length);

      assertFalse(section.$.noAddressesLabel.hidden);
      assertFalse(section.$$('#addAddress').disabled);
      assertFalse(section.$$('#autofillProfileToggle').disabled);
    });

    test('verifyAddressCount', function() {
      const addresses = [
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
        FakeDataMaker.addressEntry(),
      ];

      const section = createAutofillSection(
          addresses,
          {enabled: {value: true}, profile_enabled: {value: true}});

      const addressList = section.$.addressList;
      assertTrue(!!addressList);
      assertEquals(
          addresses.length, addressList.querySelectorAll('.list-item').length);

      assertTrue(section.$.noAddressesLabel.hidden);
      assertFalse(section.$$('#autofillProfileToggle').disabled);
      assertFalse(section.$$('#addAddress').disabled);
    });

    test('verifyAddressDisabled', function() {
      const section = createAutofillSection(
          [], {enabled: {value: true}, profile_enabled: {value: false}});

      assertFalse(section.$$('#autofillProfileToggle').disabled);
      assertTrue(section.$$('#addAddress').disabled);
    });

    test('verifyAddressFields', function() {
      const address = FakeDataMaker.addressEntry();
      const section = createAutofillSection([address], {});
      const addressList = section.$.addressList;
      const row = addressList.children[0];
      assertTrue(!!row);

      const addressSummary =
          address.metadata.summaryLabel + address.metadata.summarySublabel;

      let actualSummary = '';

      // Eliminate white space between nodes!
      const addressPieces = row.querySelector('#addressSummary').children;
      for (let i = 0; i < addressPieces.length; ++i) {
        actualSummary += addressPieces[i].textContent.trim();
      }

      assertEquals(addressSummary, actualSummary);
    });

    test('verifyAddressRowButtonIsDropdownWhenLocal', function() {
      const address = FakeDataMaker.addressEntry();
      address.metadata.isLocal = true;
      const section = createAutofillSection([address], {});
      const addressList = section.$.addressList;
      const row = addressList.children[0];
      assertTrue(!!row);
      const menuButton = row.querySelector('#addressMenu');
      assertTrue(!!menuButton);
      const outlinkButton =
          row.querySelector('paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyAddressRowButtonIsOutlinkWhenRemote', function() {
      const address = FakeDataMaker.addressEntry();
      address.metadata.isLocal = false;
      const section = createAutofillSection([address], {});
      const addressList = section.$.addressList;
      const row = addressList.children[0];
      assertTrue(!!row);
      const menuButton = row.querySelector('#addressMenu');
      assertFalse(!!menuButton);
      const outlinkButton =
          row.querySelector('paper-icon-button-light.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddAddressDialog', function() {
      return createAddressDialog(FakeDataMaker.emptyAddressEntry())
          .then(function(dialog) {
            const title = dialog.$$('[slot=title]');
            assertEquals(
                loadTimeData.getString('addAddressTitle'), title.textContent);
            // Shouldn't be possible to save until something is typed in.
            assertTrue(dialog.$.saveButton.disabled);
          });
    });

    test('verifyEditAddressDialog', function() {
      return createAddressDialog(FakeDataMaker.addressEntry())
          .then(function(dialog) {
            const title = dialog.$$('[slot=title]');
            assertEquals(
                loadTimeData.getString('editAddressTitle'), title.textContent);
            // Should be possible to save when editing because fields are
            // populated.
            assertFalse(dialog.$.saveButton.disabled);
          });
    });

    test('verifyCountryIsSaved', function() {
      const address = FakeDataMaker.emptyAddressEntry();
      return createAddressDialog(address).then(function(dialog) {
        const countrySelect = dialog.$$('select');
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
      const address = FakeDataMaker.emptyAddressEntry();
      return createAddressDialog(address).then(function(dialog) {
        assertEquals('', dialog.$.phoneInput.value);
        assertFalse(!!(address.phoneNumbers && address.phoneNumbers[0]));

        assertEquals('', dialog.$.emailInput.value);
        assertFalse(!!(address.emailAddresses && address.emailAddresses[0]));

        const phoneNumber = '(555) 555-5555';
        const emailAddress = 'no-reply@chromium.org';

        dialog.$.phoneInput.value = phoneNumber;
        dialog.$.emailInput.value = emailAddress;

        return expectEvent(dialog, 'save-address', function() {
                 dialog.$.saveButton.click();
               }).then(function() {
          assertEquals(phoneNumber, dialog.$.phoneInput.value);
          assertEquals(phoneNumber, address.phoneNumbers[0]);

          assertEquals(emailAddress, dialog.$.emailInput.value);
          assertEquals(emailAddress, address.emailAddresses[0]);
        });
      });
    });

    test('verifyPhoneAndEmailAreRemoved', function() {
      const address = FakeDataMaker.emptyAddressEntry();

      const phoneNumber = '(555) 555-5555';
      const emailAddress = 'no-reply@chromium.org';

      address.countryCode = 'US';  // Set to allow save to be active.
      address.phoneNumbers = [phoneNumber];
      address.emailAddresses = [emailAddress];

      return createAddressDialog(address).then(function(dialog) {
        assertEquals(phoneNumber, dialog.$.phoneInput.value);
        assertEquals(emailAddress, dialog.$.emailInput.value);

        dialog.$.phoneInput.value = '';
        dialog.$.emailInput.value = '';

        return expectEvent(dialog, 'save-address', function() {
                 dialog.$.saveButton.click();
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
      return createAddressDialog(FakeDataMaker.emptyAddressEntry())
          .then(function(dialog) {
            const saveButton = dialog.$.saveButton;
            const testElements =
                dialog.$.dialog.querySelectorAll('settings-textarea, cr-input');

            // Default country is 'US' expecting: Name, Organization,
            // Street address, City, State, ZIP code, Phone, and Email.
            assertEquals(8, testElements.length);

            return asyncForEach(testElements, function(element) {
              return expectEvent(
                         dialog, 'on-update-can-save',
                         function() {
                           assertTrue(saveButton.disabled);
                           element.value = 'foo';
                         })
                  .then(function() {
                    return expectEvent(
                        dialog, 'on-update-can-save', function() {
                          assertFalse(saveButton.disabled);
                          element.value = '';
                        });
                  })
                  .then(function() {
                    assertTrue(saveButton.disabled);
                  });
            });
          });
    });

    // Setting the country should allow the address to be saved.
    test('verifySaveIsNotClickableIfCountryNotSet', function() {
      let dialog = null;

      const simulateCountryChange = function(countryCode) {
        const countrySelect = dialog.$$('select');
        countrySelect.value = countryCode;
        countrySelect.dispatchEvent(new CustomEvent('change'));
      };

      return createAddressDialog(FakeDataMaker.emptyAddressEntry())
          .then(function(d) {
            dialog = d;
            assertTrue(dialog.$.saveButton.disabled);

            return expectEvent(
                dialog, 'on-update-can-save',
                simulateCountryChange.bind(null, 'US'));
          })
          .then(function() {
            assertFalse(dialog.$.saveButton.disabled);

            return expectEvent(
                dialog, 'on-update-can-save',
                simulateCountryChange.bind(null, ''));
          })
          .then(function() {
            assertTrue(dialog.$.saveButton.disabled);
          });
    });

    // Test will timeout if save-address event is not fired.
    test('verifyDefaultCountryIsAppliedWhenSaving', function() {
      const address = FakeDataMaker.emptyAddressEntry();
      address.companyName = 'Google';
      return createAddressDialog(address).then(function(dialog) {
        return expectEvent(dialog, 'save-address', function() {
                 // Verify |countryCode| is not set.
                 assertEquals(undefined, address.countryCode);
                 dialog.$.saveButton.click();
               }).then(function(event) {
          // 'US' is the default country for these tests.
          assertEquals('US', event.detail.countryCode);
        });
      });
    });

    test('verifyCancelDoesNotSaveAddress', function(done) {
      createAddressDialog(FakeDataMaker.addressEntry())
          .then(function(dialog) {
            test_util.eventToPromise('save-address', dialog).then(function() {
              // Fail the test because the save event should not be called when
              // cancel is clicked.
              assertTrue(false);
            });

            test_util.eventToPromise('close', dialog).then(function() {
              // Test is |done| in a timeout in order to ensure that
              // 'save-address' is NOT fired after this test.
              window.setTimeout(done, 100);
            });

            dialog.$.cancelButton.click();
          });
    });
  });

  suite('AutofillSectionAddressLocaleTests', function() {
    suiteSetup(function() {
      settings.address.CountryDetailManagerImpl.instance_ =
          new CountryDetailManagerTestImpl();
    });

    setup(function() {
      PolymerTest.clearBody();
    });

    // US address has 3 fields on the same line.
    test('verifyEditingUSAddress', function() {
      const address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = ['Name'];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'City';
      address.addressLevel1 = 'State';
      address.postalCode = 'ZIP code';
      address.countryCode = 'US';
      address.phoneNumbers = ['Phone'];
      address.emailAddresses = ['Email'];

      return createAddressDialog(address).then(function(dialog) {
        const rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(6, rows.length);

        // Name
        let row = rows[0];
        let cols = row.querySelectorAll('.address-column');
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
        const countrySelect = row.querySelector('select');
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
      const address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = ['Name'];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'Post town';
      address.postalCode = 'Postal code';
      address.countryCode = 'GB';
      address.phoneNumbers = ['Phone'];
      address.emailAddresses = ['Email'];

      return createAddressDialog(address).then(function(dialog) {
        const rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(7, rows.length);

        // Name
        let row = rows[0];
        let cols = row.querySelectorAll('.address-column');
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
        const countrySelect = row.querySelector('select');
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
      const address = FakeDataMaker.emptyAddressEntry();

      address.fullNames = ['Name'];
      address.companyName = 'Organization';
      address.addressLines = 'Street address';
      address.addressLevel2 = 'City';
      address.postalCode = 'Postal code';
      address.countryCode = 'IL';
      address.phoneNumbers = ['Phone'];
      address.emailAddresses = ['Email'];

      return createAddressDialog(address).then(function(dialog) {
        const rows = dialog.$.dialog.querySelectorAll('.address-row');
        assertEquals(6, rows.length);

        // Name
        let row = rows[0];
        let cols = row.querySelectorAll('.address-column');
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
        const countrySelect = row.querySelector('select');
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
      const address = FakeDataMaker.emptyAddressEntry();
      address.countryCode = 'US';

      return createAddressDialog(address).then(function(dialog) {
        const city = 'Los Angeles';
        const state = 'CA';
        const zip = '90291';
        const countrySelect = dialog.$$('select');

        return expectEvent(
                   dialog, 'on-update-address-wrapper',
                   function() {
                     // US:
                     const rows =
                         dialog.$.dialog.querySelectorAll('.address-row');
                     assertEquals(6, rows.length);

                     // City, State, ZIP code
                     const row = rows[3];
                     const cols = row.querySelectorAll('.address-column');
                     assertEquals(3, cols.length);
                     cols[0].value = city;
                     cols[1].value = state;
                     cols[2].value = zip;

                     countrySelect.value = 'IL';
                     countrySelect.dispatchEvent(new CustomEvent('change'));
                   })
            .then(function() {
              return expectEvent(
                  dialog, 'on-update-address-wrapper', function() {
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
            })
            .then(function() {
              // US:
              const rows = dialog.$.dialog.querySelectorAll('.address-row');
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
});
