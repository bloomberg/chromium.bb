// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_sync_page', function() {
  /**
   * @constructor
   * @implements {settings.SyncBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestSyncBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'didNavigateToSyncPage',
      'didNavigateAwayFromSyncPage',
      'setSyncDatatypes',
      'setSyncEncryption',
    ]);

    /* @type {!settings.PageStatus} */
    this.encryptionResponse = settings.PageStatus.CONFIGURE;
  };

  TestSyncBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    didNavigateToSyncPage: function() {
      this.methodCalled('didNavigateToSyncPage');
    },

    /** @override */
    didNavigateAwayFromSyncPage: function() {
      this.methodCalled('didNavigateAwayFromSyncPage');
    },

    /** @override */
    setSyncDatatypes: function(syncPrefs) {
      this.methodCalled('setSyncDatatypes', syncPrefs);
      return Promise.resolve(settings.PageStatus.CONFIGURE);
    },

    /** @override */
    setSyncEncryption: function(syncPrefs) {
      this.methodCalled('setSyncEncryption', syncPrefs);
      return Promise.resolve(this.encryptionResponse);
    },
  };

  function registerAdvancedSyncSettingsTests() {
    suite('AdvancedSyncSettingsTests', function() {
      var syncPage = null;
      var browserProxy = null;
      var encryptWithGoogle = null;
      var encyyptWithPassphrase = null;

      /**
       * Returns sync prefs with everything synced and no passphrase required.
       * @return {!settings.SyncPrefs}
       */
      function getSyncAllPrefs() {
        return {
          appsEnforced: false,
          appsRegistered: true,
          appsSynced: true,
          autofillEnforced: false,
          autofillRegistered: true,
          autofillSynced: true,
          bookmarksEnforced: false,
          bookmarksRegistered: true,
          bookmarksSynced: true,
          encryptAllData: false,
          encryptAllDataAllowed: true,
          enterGooglePassphraseBody: 'Enter Google passphrase.',
          enterPassphraseBody: 'Enter custom passphrase.',
          extensionsEnforced: false,
          extensionsRegistered: true,
          extensionsSynced: true,
          fullEncryptionBody: '',
          passphrase: '',
          passphraseRequired: false,
          passphraseTypeIsCustom: false,
          passwordsEnforced: false,
          passwordsRegistered: true,
          passwordsSynced: true,
          paymentsIntegrationEnabled: true,
          preferencesEnforced: false,
          preferencesRegistered: true,
          preferencesSynced: true,
          setNewPassphrase: false,
          syncAllDataTypes: true,
          tabsEnforced: false,
          tabsRegistered: true,
          tabsSynced: true,
          themesEnforced: false,
          themesRegistered: true,
          themesSynced: true,
          typedUrlsEnforced: false,
          typedUrlsRegistered: true,
          typedUrlsSynced: true,
        };
      }

      setup(function() {
        browserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        syncPage = document.createElement('settings-sync-page');
        settings.navigateTo(settings.Route.SYNC);

        document.body.appendChild(syncPage);

        cr.webUIListenerCallback('page-status-changed',
                                 settings.PageStatus.CONFIGURE);
        assertFalse(syncPage.$$('#' + settings.PageStatus.CONFIGURE).hidden);
        assertTrue(syncPage.$$('#' + settings.PageStatus.TIMEOUT).hidden);
        assertTrue(syncPage.$$('#' + settings.PageStatus.SPINNER).hidden);

        // Start with Sync All with no encryption selected.
        cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
        Polymer.dom.flush();

        encryptWithGoogle =
            syncPage.$$('paper-radio-button[name="encrypt-with-google"]');
        encryptWithPassphrase =
            syncPage.$$('paper-radio-button[name="encrypt-with-passphrase"]');
        assertTrue(!!encryptWithGoogle);
        assertTrue(!!encryptWithPassphrase);
      });

      teardown(function() { syncPage.remove(); });

      test('NotifiesHandlerOfNavigation', function() {
        function testNavigateAway() {
          settings.navigateTo(settings.Route.PEOPLE);
          return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
        }

        function testNavigateBack() {
          browserProxy.resetResolver('didNavigateToSyncPage');
          settings.navigateTo(settings.Route.SYNC);
          return browserProxy.whenCalled('didNavigateToSyncPage');
        }

        function testDetach() {
          browserProxy.resetResolver('didNavigateAwayFromSyncPage');
          syncPage.remove();
          return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
        }

        function testRecreate() {
          browserProxy.resetResolver('didNavigateToSyncPage');
          syncPage = document.createElement('settings-sync-page');
          settings.navigateTo(settings.Route.SYNC);

          document.body.appendChild(syncPage);
          return browserProxy.whenCalled('didNavigateToSyncPage');
        }

        return browserProxy.whenCalled('didNavigateToSyncPage')
            .then(testNavigateAway)
            .then(testNavigateBack)
            .then(testDetach)
            .then(testRecreate);
      }),

      test('LoadingAndTimeout', function() {
        var configurePage = syncPage.$$('#' + settings.PageStatus.CONFIGURE);
        var spinnerPage = syncPage.$$('#' + settings.PageStatus.SPINNER);
        var timeoutPage = syncPage.$$('#' + settings.PageStatus.TIMEOUT);

        cr.webUIListenerCallback('page-status-changed',
                                 settings.PageStatus.SPINNER);
        assertTrue(configurePage.hidden);
        assertTrue(timeoutPage.hidden);
        assertFalse(spinnerPage.hidden);

        cr.webUIListenerCallback('page-status-changed',
                                 settings.PageStatus.TIMEOUT);
        assertTrue(configurePage.hidden);
        assertFalse(timeoutPage.hidden);
        assertTrue(spinnerPage.hidden);

        cr.webUIListenerCallback('page-status-changed',
                                 settings.PageStatus.CONFIGURE);
        assertFalse(configurePage.hidden);
        assertTrue(timeoutPage.hidden);
        assertTrue(spinnerPage.hidden);

        // Should remain on the CONFIGURE page even if the passphrase failed.
        cr.webUIListenerCallback('page-status-changed',
                                 settings.PageStatus.PASSPHRASE_FAILED);
        assertFalse(configurePage.hidden);
        assertTrue(timeoutPage.hidden);
        assertTrue(spinnerPage.hidden);
      });

      test('SettingIndividualDatatypes', function() {
        var syncAllDataTypesCheckbox = syncPage.$.syncAllDataTypesCheckbox;
        assertFalse(syncAllDataTypesCheckbox.disabled);
        assertTrue(syncAllDataTypesCheckbox.checked);

        // Assert that all the individual datatype checkboxes are disabled.
        var datatypeCheckboxes = syncPage
            .$$('#configure')
            .querySelectorAll('paper-checkbox.list-item');
        for (var box of datatypeCheckboxes) {
          assertTrue(box.disabled);
          assertTrue(box.checked);
        }

        // Uncheck the Sync All checkbox.
        MockInteractions.tap(syncAllDataTypesCheckbox);

        function verifyPrefs(prefs) {
          var expected = getSyncAllPrefs();
          expected.syncAllDataTypes = false;
          assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

          cr.webUIListenerCallback('sync-prefs-changed', expected);

          // Assert that all the individual datatype checkboxes are enabled.
          for (var box of datatypeCheckboxes) {
            assertFalse(box.disabled);
            assertTrue(box.checked);
          }

          browserProxy.resetResolver('setSyncDatatypes');

          // Test an arbitrarily-selected checkbox (extensions synced checkbox).
          MockInteractions.tap(datatypeCheckboxes[3]);
          return browserProxy.whenCalled('setSyncDatatypes').then(
              function(prefs) {
                var expected = getSyncAllPrefs();
                expected.syncAllDataTypes = false;
                expected.extensionsSynced = false;
                assertEquals(JSON.stringify(expected), JSON.stringify(prefs));
              });
        }
        return browserProxy.whenCalled('setSyncDatatypes').then(verifyPrefs);
      });

      test('ClickingLinkDoesNotChangeCheckboxValue', function() {
        var syncAllDataTypesCheckbox = syncPage.$.syncAllDataTypesCheckbox;

        // Uncheck the Sync All checkbox.
        MockInteractions.tap(syncAllDataTypesCheckbox);

        // Make sure the checkbox is enabled and checked.
        var link = syncPage.$.paymentLearnMore;

        // Suppress opening a new tab, since then the test will continue running
        // on a background tab (which has throttled timers) and will timeout.
        link.target = '';
        link.href = '#';

        assertEquals('PAPER-CHECKBOX', link.parentNode.nodeName);
        assertFalse(link.parentNode.disabled);
        assertTrue(link.parentNode.checked);

        MockInteractions.tap(link);

        // The checkbox value should be unchanged after clicking on the link.
        assertTrue(link.parentNode.checked);
      });

      test('RadioBoxesEnabledWhenUnencrypted', function() {
        // Verify that the encryption radio boxes are enabled.
        assertFalse(encryptWithGoogle.disabled);
        assertFalse(encryptWithPassphrase.disabled);

        assertTrue(encryptWithGoogle.checked);

        // Select 'Encrypt with passphrase' to create a new passphrase.
        assertFalse(!!syncPage.$$('#create-password-box'));

        MockInteractions.tap(encryptWithPassphrase);
        Polymer.dom.flush();

        assertTrue(!!syncPage.$$('#create-password-box'));
        var saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
        assertTrue(!!saveNewPassphrase);

        // Test that a sync prefs update does not reset the selection.
        cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
        Polymer.dom.flush();
        assertTrue(encryptWithPassphrase.checked);
      });

      test('SaveButtonDisabledWhenPassphraseOrConfirmationEmpty', function() {
        MockInteractions.tap(encryptWithPassphrase);
        Polymer.dom.flush();

        assertTrue(!!syncPage.$$('#create-password-box'));
        var saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
        var passphraseInput = syncPage.$$('#passphraseInput');
        var passphraseConfirmationInput =
            syncPage.$$('#passphraseConfirmationInput');

        passphraseInput.value = '';
        passphraseConfirmationInput.value = '';
        assertTrue(saveNewPassphrase.disabled);

        passphraseInput.value = 'foo';
        passphraseConfirmationInput.value = '';
        assertTrue(saveNewPassphrase.disabled);

        passphraseInput.value = 'foo';
        passphraseConfirmationInput.value = 'bar';
        assertFalse(saveNewPassphrase.disabled);
      });

      test('CreatingPassphraseMismatchedPassphrase', function() {
        MockInteractions.tap(encryptWithPassphrase);
        Polymer.dom.flush();

        assertTrue(!!syncPage.$$('#create-password-box'));
        var saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
        assertTrue(!!saveNewPassphrase);

        var passphraseInput = syncPage.$$('#passphraseInput');
        var passphraseConfirmationInput =
            syncPage.$$('#passphraseConfirmationInput');
        passphraseInput.value = 'foo';
        passphraseConfirmationInput.value = 'bar';

        MockInteractions.tap(saveNewPassphrase);
        Polymer.dom.flush();

        assertFalse(passphraseInput.invalid);
        assertTrue(passphraseConfirmationInput.invalid);

        assertFalse(syncPage.syncPrefs.encryptAllData);
      });

      test('CreatingPassphraseValidPassphrase', function() {
        MockInteractions.tap(encryptWithPassphrase);
        Polymer.dom.flush();

        assertTrue(!!syncPage.$$('#create-password-box'));
        var saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
        assertTrue(!!saveNewPassphrase);

        var passphraseInput = syncPage.$$('#passphraseInput');
        var passphraseConfirmationInput =
            syncPage.$$('#passphraseConfirmationInput');
        passphraseInput.value = 'foo';
        passphraseConfirmationInput.value = 'foo';
        MockInteractions.tap(saveNewPassphrase);

        function verifyPrefs(prefs) {
          var expected = getSyncAllPrefs();
          expected.setNewPassphrase = true;
          expected.passphrase = 'foo';
          expected.encryptAllData = true;
          assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

          expected.fullEncryptionBody = 'Encrypted with custom passphrase';
          cr.webUIListenerCallback('sync-prefs-changed', expected);

          Polymer.dom.flush();

          // Assert that the radio boxes are disabled after encryption enabled.
          assertTrue(encryptWithGoogle.disabled);
          assertTrue(encryptWithPassphrase.disabled);
        }
        return browserProxy.whenCalled('setSyncEncryption').then(verifyPrefs);
      });

      test('RadioBoxesHiddenWhenEncrypted', function() {
        var prefs = getSyncAllPrefs();
        prefs.encryptAllData = true;
        prefs.passphraseRequired = true;
        prefs.fullEncryptionBody = 'Sync already encrypted.';
        cr.webUIListenerCallback('sync-prefs-changed', prefs);

        Polymer.dom.flush();

        assertTrue(syncPage.$.encryptionRadioGroupContainer.hidden);
      });

      test('ExistingPassphraseSubmitButtonDisabledWhenExistingPassphraseEmpty',
           function() {
        var prefs = getSyncAllPrefs();
        prefs.encryptAllData = true;
        prefs.passphraseRequired = true;
        cr.webUIListenerCallback('sync-prefs-changed', prefs);

        Polymer.dom.flush();

        var existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
        var submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');

        existingPassphraseInput.value = '';
        assertTrue(submitExistingPassphrase.disabled);

        existingPassphraseInput.value = 'foo';
        assertFalse(submitExistingPassphrase.disabled);
      });

      test('EnterExistingWrongPassphrase', function() {
        var prefs = getSyncAllPrefs();
        prefs.encryptAllData = true;
        prefs.passphraseRequired = true;
        cr.webUIListenerCallback('sync-prefs-changed', prefs);

        Polymer.dom.flush();

        var existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
        assertTrue(!!existingPassphraseInput);
        existingPassphraseInput.value = 'wrong';
        browserProxy.encryptionResponse = settings.PageStatus.PASSPHRASE_FAILED;

        var submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
        assertTrue(!!submitExistingPassphrase);
        MockInteractions.tap(submitExistingPassphrase);

        return browserProxy.whenCalled('setSyncEncryption').then(
            function(prefs) {
              var expected = getSyncAllPrefs();
              expected.setNewPassphrase = false;
              expected.passphrase = 'wrong';
              expected.encryptAllData = true;
              expected.passphraseRequired = true;
              assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

              Polymer.dom.flush();

              assertTrue(existingPassphraseInput.invalid);
            });
      });

      test('EnterExistingCorrectPassphrase', function() {
        var prefs = getSyncAllPrefs();
        prefs.encryptAllData = true;
        prefs.passphraseRequired = true;
        cr.webUIListenerCallback('sync-prefs-changed', prefs);

        Polymer.dom.flush();

        var existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
        assertTrue(!!existingPassphraseInput);
        existingPassphraseInput.value = 'right';
        browserProxy.encryptionResponse = settings.PageStatus.CONFIGURE;

        var submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
        assertTrue(!!submitExistingPassphrase);
        MockInteractions.tap(submitExistingPassphrase);

        return browserProxy.whenCalled('setSyncEncryption').then(
            function(prefs) {
              var expected = getSyncAllPrefs();
              expected.setNewPassphrase = false;
              expected.passphrase = 'right';
              expected.encryptAllData = true;
              expected.passphraseRequired = true;
              assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

              var newPrefs = getSyncAllPrefs();
              newPrefs.encryptAllData = true;
              cr.webUIListenerCallback('sync-prefs-changed', newPrefs);

              Polymer.dom.flush();

              // Verify that the encryption radio boxes are shown but disabled.
              assertTrue(encryptWithGoogle.disabled);
              assertTrue(encryptWithPassphrase.disabled);
            });
      });
    });
  }

  return {
    registerTests: function() {
      registerAdvancedSyncSettingsTests();
    },
  };
});
