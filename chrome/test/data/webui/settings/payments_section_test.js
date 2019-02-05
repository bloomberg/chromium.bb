// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_payments_section', function() {
  suite('PaymentSectionUiTest', function() {
    test('testAutofillExtensionIndicator', function() {
      // Initializing with fake prefs
      const section = document.createElement('settings-payments-section');
      section.prefs = {autofill: {credit_card_enabled: {}}};
      document.body.appendChild(section);

      assertFalse(!!section.$$('#autofillExtensionIndicator'));
      section.set('prefs.autofill.credit_card_enabled.extensionId', 'test-id');
      Polymer.dom.flush();

      assertTrue(!!section.$$('#autofillExtensionIndicator'));
    });
  });

  suite('PaymentsSection', function() {
    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy = null;

    setup(function() {
      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
      PolymerTest.clearBody();
      loadTimeData.overrideValues({
        migrationEnabled: true,
        hasGooglePaymentsAccount: true,
        upstreamEnabled: true,
        isUsingSecondaryPassphrase: false,
        uploadToGoogleActive: true,
        userEmailDomainAllowed: true,
        splitCreditCardList: false
      });
    });

    /**
     * Creates the payments autofill section for the given list.
     * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
     * @param {!Object} prefValues
     * @return {!Object}
     */
    function createPaymentsSection(creditCards, prefValues) {
      // Override the PaymentsManagerImpl for testing.
      const paymentsManager = new TestPaymentsManager();
      paymentsManager.data.creditCards = creditCards;
      PaymentsManagerImpl.instance_ = paymentsManager;

      const section = document.createElement('settings-payments-section');
      section.prefs = {autofill: prefValues};
      document.body.appendChild(section);
      Polymer.dom.flush();

      return section;
    }

    /**
     * Creates the payments autofill section for the given lists.
     * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} localCreditCards
     * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>}
     *     serverCreditCards
     * @return {!Object}
     */
    function createSplitPaymentsSection(localCreditCards, serverCreditCards) {
      loadTimeData.overrideValues({splitCreditCardList: true});

      // Override the PaymentsManagerImpl for testing.
      const paymentsManager = new TestPaymentsManager();
      paymentsManager.data.localCreditCards = localCreditCards;
      paymentsManager.data.serverCreditCards = serverCreditCards;
      PaymentsManagerImpl.instance_ = paymentsManager;

      const section = document.createElement('settings-payments-section');
      document.body.appendChild(section);
      Polymer.dom.flush();

      return section;
    }

    /**
     * Creates the Edit Credit Card dialog.
     * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
     * @return {!Object}
     */
    function createCreditCardDialog(creditCardItem) {
      const section =
          document.createElement('settings-credit-card-edit-dialog');
      section.creditCard = creditCardItem;
      document.body.appendChild(section);
      Polymer.dom.flush();
      return section;
    }

    /**
     * Returns an array containing the local and server credit card items.
     * @return {!Array<!chrome.autofillPrivate.CreditCardEntry>}
     */
    function getLocalAndServerListItems() {
      return document.body.querySelector('settings-payments-section')
          .$$('#creditCardList')
          .shadowRoot.querySelectorAll('settings-credit-card-list-entry');
    }

    /**
     * Returns an array containing the local credit card items.
     * @return {!Array<!chrome.autofillPrivate.CreditCardEntry>}
     */
    function getLocalListItems() {
      return document.body.querySelector('settings-payments-section')
          .$$('#localCreditCardList')
          .shadowRoot.querySelectorAll('settings-credit-card-list-entry');
    }

    /**
     * Returns an array containing the server credit card items.
     * @return {!Array<!chrome.autofillPrivate.CreditCardEntry>}
     */
    function getServerListItems() {
      return document.body.querySelector('settings-payments-section')
          .$$('#serverCreditCardList')
          .shadowRoot.querySelectorAll('settings-credit-card-list-entry');
    }

    /**
     * Makes sure that the number of actual local and server credit cards
     * match the given expectations.
     * @param {number} expectedServer
     * @param {number} expectedLocal
     */
    function assertCreditCards(expectedLocal, expectedServer) {
      assertEquals(expectedLocal, getLocalListItems().length);
      assertEquals(expectedServer, getServerListItems().length);
    }

    /**
     * Returns the shadow root of the card row from the specified card list.
     * @param {!HTMLElement} cardList
     * @return {?HTMLElement}
     */
    function getCardRowShadowRoot(cardList) {
      const row = cardList.$$('settings-credit-card-list-entry');
      assertTrue(!!row);
      return row.shadowRoot;
    }

    test('verifyCreditCardCount', function() {
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: true}});

      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(0, getLocalAndServerListItems().length);

      assertFalse(creditCardList.$$('#noCreditCardsLabel').hidden);
      assertTrue(creditCardList.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardsDisabled', function() {
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: false}});

      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertTrue(section.$$('#addCreditCard').hidden);
    });

    test('verifyCreditCardCount', function() {
      const creditCards = [
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
      ];

      const section = createPaymentsSection(
          creditCards, {credit_card_enabled: {value: true}});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(creditCards.length, getLocalAndServerListItems().length);

      assertTrue(creditCardList.$$('#noCreditCardsLabel').hidden);
      assertFalse(creditCardList.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardFields', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertEquals(
          creditCard.metadata.summaryLabel,
          rowShadowRoot.querySelector('#creditCardLabel').textContent);
      assertEquals(
          creditCard.expirationMonth + '/' + creditCard.expirationYear,
          rowShadowRoot.querySelector('#creditCardExpiration')
              .textContent.trim());
    });

    test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = true;
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = false;
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertFalse(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('paper-icon-button-light.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddVsEditCreditCardTitle', function() {
      const newCreditCard = FakeDataMaker.emptyCreditCardEntry();
      const newCreditCardDialog = createCreditCardDialog(newCreditCard);
      const oldCreditCard = FakeDataMaker.creditCardEntry();
      const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

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
      const creditCard = FakeDataMaker.creditCardEntry();

      // 2015 is over unless time goes wobbly.
      const twentyFifteen = 2015;
      creditCard.expirationYear = twentyFifteen.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const now = new Date();
            const maxYear = now.getFullYear() + 19;
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals('2015', yearOptions[0].textContent.trim());
            assertEquals(
                maxYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verifyVeryFutureCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 25 years from now is unusual.
      const now = new Date();
      const farFutureYear = now.getFullYear() + 25;
      creditCard.expirationYear = farFutureYear.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                farFutureYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verifyVeryNormalCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 2 years from now is not unusual.
      const now = new Date();
      const nearFutureYear = now.getFullYear() + 2;
      creditCard.expirationYear = nearFutureYear.toString();
      const maxYear = now.getFullYear() + 19;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                maxYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verify save disabled for expired credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();

      const now = new Date();
      creditCard.expirationYear = now.getFullYear() - 2;
      // works fine for January.
      creditCard.expirationMonth = now.getMonth() - 1;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            assertTrue(creditCardDialog.$.saveButton.disabled);
          });
    });

    test('verify save new credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').
            then(function() {
              // Not expired, but still can't be saved, because there's no
              // name.
              assertTrue(creditCardDialog.$.expired.hidden);
              assertTrue(creditCardDialog.$.saveButton.disabled);

              // Add a name and trigger the on-input handler.
              creditCardDialog.set('creditCard.name', 'Jane Doe');
              creditCardDialog.onCreditCardNameOrNumberChanged_();
              Polymer.dom.flush();

              assertTrue(creditCardDialog.$.expired.hidden);
              assertFalse(creditCardDialog.$.saveButton.disabled);

              const savedPromise = test_util.eventToPromise('save-credit-card',
                  creditCardDialog);
              creditCardDialog.$.saveButton.click();
              return savedPromise;
            }).then(function(event) {
              assertEquals(creditCard.guid, event.detail.guid);
            });
    });

    test('verifyCancelCreditCardEdit', function(done) {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            test_util.eventToPromise('save-credit-card', creditCardDialog)
              .then(function() {
                // Fail the test because the save event should not be called
                // when cancel is clicked.
                assertTrue(false);
                done();
              });

            test_util.eventToPromise('close', creditCardDialog)
              .then(function() {
                // Test is |done| in a timeout in order to ensure that
                // 'save-credit-card' is NOT fired after this test.
                window.setTimeout(done, 100);
              });

            creditCardDialog.$.cancelButton.click();
          });
    });

    test('verifyLocalCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // When credit card is local, |isCached| will be undefined.
      creditCard.metadata.isLocal = true;
      creditCard.metadata.isCached = undefined;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // Local credit cards will show the overflow menu.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertFalse(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertTrue(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = true;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // Cached remote CCs will show overflow menu.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertFalse(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyNotCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = false;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // No overflow menu when not cached.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
    });

    test('verifyLocalCreditCardInSplitSettings', function() {
      // Create a local credit card.
      const localCard = FakeDataMaker.creditCardEntry();
      assertTrue(localCard.metadata.isLocal);

      const section = createSplitPaymentsSection([localCard], []);
      assertCreditCards(1 /*expectedLocal*/, 0 /*expectedServer*/);

      // Make sure the button is a dropdown and not an outlink.
      const rowShadowRoot =
          getCardRowShadowRoot(section.$$('#localCreditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyMaskedServerCreditCardInSplitSettings', function() {
      // Create a server credit card.
      const maskedServerCard = FakeDataMaker.creditCardEntry();
      maskedServerCard.metadata.isLocal = false;
      maskedServerCard.metadata.isCached = false;

      const section = createSplitPaymentsSection([], [maskedServerCard]);
      assertCreditCards(0 /*expectedLocal*/, 1 /*expectedServer*/);

      // Make sure the button is an outlink and not a dropdown.
      const rowShadowRoot =
          getCardRowShadowRoot(section.$$('#serverCreditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertFalse(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('paper-icon-button-light.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyUnmaskedServerCreditCardInSplitSettings', function() {
      // Create a full (unmasked) server credit card.
      const fullServerCard = FakeDataMaker.creditCardEntry();
      fullServerCard.metadata.isLocal = false;
      fullServerCard.metadata.isCached = true;

      const section =
          createSplitPaymentsSection([fullServerCard], [fullServerCard]);

      // Make sure the card is present in the two sections.
      assertCreditCards(1 /*expectedLocal*/, 1 /*expectedServer*/);

      // Make sure the button is a dropdown and not an outlink for both the
      // local and server sections.
      const localRowShadowRoot =
          getCardRowShadowRoot(section.$$('#localCreditCardList'));
      menuButton = localRowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      outlinkButton = localRowShadowRoot.querySelector(
          'paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);

      const serverRowShadowRoot =
          getCardRowShadowRoot(section.$$('#serverCreditCardList'));
      menuButton = serverRowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      outlinkButton = serverRowShadowRoot.querySelector(
          'paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyLocalCreditCardMenu_SplitSettings', function() {
      // Create a local credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      assertTrue(creditCard.metadata.isLocal);

      const section = createSplitPaymentsSection([creditCard], []);
      assertEquals(1, getLocalListItems().length);

      // Local credit cards will show the overflow menu.
      const localRowShadowRoot =
          getCardRowShadowRoot(section.$$('#localCreditCardList'));
      assertFalse(!!localRowShadowRoot.querySelector('#remoteCreditCardLink'));
      const menuButton = localRowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      // Make sure only the two expected options are in the menu.
      menuButton.click();
      Polymer.dom.flush();
      const menu = section.$.creditCardSharedMenu;
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertFalse(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertTrue(menu.querySelector('#menuClearCreditCard').hidden);
      menu.close();
      Polymer.dom.flush();
    });

    test('verifyCachedCreditCardMenu_SplitSettings', function() {
      // Create a full (cached) credit card.
      const fullServerCard = FakeDataMaker.creditCardEntry();
      fullServerCard.metadata.isLocal = false;
      fullServerCard.metadata.isCached = true;

      const section =
          createSplitPaymentsSection([fullServerCard], [fullServerCard]);

      // Make sure the card is present in the two sections.
      assertCreditCards(1 /*expectedLocal*/, 1 /*expectedServer*/);

      // Make sure the button in the local section is a menu.
      const localRowShadowRoot =
          getCardRowShadowRoot(section.$$('#localCreditCardList'));
      assertFalse(!!localRowShadowRoot.querySelector('#remoteCreditCardLink'));
      menuButton = localRowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      outlinkButton = localRowShadowRoot.querySelector(
          'paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);

      // Make sure only the two expected options are in the menu.
      menuButton.click();
      Polymer.dom.flush();
      menu = section.$.creditCardSharedMenu;
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertFalse(menu.querySelector('#menuClearCreditCard').hidden);
      menu.close();
      Polymer.dom.flush();

      // Make sure the button in the local section is also a menu.
      const serverRowShadowRoot =
          getCardRowShadowRoot(section.$$('#serverCreditCardList'));
      assertFalse(!!serverRowShadowRoot.querySelector('#remoteCreditCardLink'));
      menuButton = serverRowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      outlinkButton = serverRowShadowRoot.querySelector(
          'paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);

      // Make sure only the two expected options are in the menu.
      menuButton.click();
      Polymer.dom.flush();
      menu = section.$.creditCardSharedMenu;
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertFalse(menu.querySelector('#menuClearCreditCard').hidden);
      menu.close();
      Polymer.dom.flush();
    });

    test('verifyNotCachedCreditCardMenu_SplitSettings', function() {
      // Create a masked (not cached) credit card.
      const maskedCard = FakeDataMaker.creditCardEntry();
      maskedCard.metadata.isLocal = false;
      maskedCard.metadata.isCached = false;

      const section = createSplitPaymentsSection([], [maskedCard]);
      assertEquals(1, getServerListItems().length);

      // No overflow menu when not cached.
      const serverRowShadowRoot =
          getCardRowShadowRoot(section.$$('#serverCreditCardList'));
      assertTrue(!!serverRowShadowRoot.querySelector('#remoteCreditCardLink'));
      assertFalse(!!serverRowShadowRoot.querySelector('#creditCardMenu'));
    });

    test('verifyMigrationButtonNotShownIfMigrationNotEnabled', function() {
      // Mock the Google Payments account. Disable the migration experimental
      // flag. Won't show migration button.
      loadTimeData.overrideValues({migrationEnabled: false});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but migration experimental flag is
      // not enabled, verify migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNotSignedIn', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate not Signed-in status. Won't show migration button.
      sync_test_util.simulateSyncStatus({
        signedIn: false,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but not signed in, verify migration
      // button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNotSynced', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate not Synced status. Won't show migration button.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: false,
      });

      // All migration requirements are met but not Synced, verify migration
      // button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNoMigratableCard', function() {
      // Add one credit card but not migratable. Won't show migration button.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = false;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but no migratable credi card, verify
      // migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownWhenCreditCardDisabled', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: false}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but credit card is disable, verify
      // migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNoGooglePaymentsAccount', function() {
      // Mocks no Google payments account. Won't show migration button.
      loadTimeData.overrideValues({hasGooglePaymentsAccount: false});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but no Google Payments account,
      // verify migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfAutofillUpstreamDisabled', function() {
      loadTimeData.overrideValues({upstreamEnabled: false});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met but Autofill Upstream is disabled,
      // verify migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test(
        'verifyMigrationButtonNotShownIfUserHasSecondaryPassphrase',
        function() {
          loadTimeData.overrideValues({isUsingSecondaryPassphrase: true});

          // Add one migratable credit card.
          const creditCard = FakeDataMaker.creditCardEntry();
          creditCard.metadata.isMigratable = true;
          const section = createPaymentsSection(
              [creditCard], {credit_card_enabled: {value: true}});

          // Simulate Signed-in and Synced status.
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            syncSystemEnabled: true,
          });

          // All migration requirements are met but the user has a secondary
          // passphrase, verify migration button is hidden.
          assertTrue(section.$$('#migrateCreditCards').hidden);
        });

    test(
        'verifyMigrationButtonNotShownIfUploadToGoogleStateIsInactive',
        function() {
          loadTimeData.overrideValues({uploadToGoogleActive: false});

          // Add one migratable credit card.
          const creditCard = FakeDataMaker.creditCardEntry();
          creditCard.metadata.isMigratable = true;
          const section = createPaymentsSection(
              [creditCard], {credit_card_enabled: {value: true}});

          // Simulate Signed-in and Synced status.
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            syncSystemEnabled: true,
          });

          // All migration requirements are met but upload to Google is
          // inactive, verify migration button is hidden.
          assertTrue(section.$$('#migrateCreditCards').hidden);
        });

    test(
        'verifyMigrationButtonNotShownIfUserEmailDomainIsNotAllowed',
        function() {
          loadTimeData.overrideValues({userEmailDomainAllowed: false});

          // Add one migratable credit card.
          const creditCard = FakeDataMaker.creditCardEntry();
          creditCard.metadata.isMigratable = true;
          const section = createPaymentsSection(
              [creditCard], {credit_card_enabled: {value: true}});

          // Simulate Signed-in and Synced status.
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            syncSystemEnabled: true,
          });

          // All migration requirements are met but the user's email domain is
          // not allowed, verify migration button is hidden.
          assertTrue(section.$$('#migrateCreditCards').hidden);
        });

    test('verifyMigrationButtonShown', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements are met, verify migration button is shown.
      assertFalse(section.$$('#migrateCreditCards').hidden);
    });
  });
});
