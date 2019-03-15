// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ChangeDropdownState: 'change dropdown state',
    NoRecentDestinations: 'no recent destinations',
    RecentDestinations: 'recent destinations',
    SaveAsPdfRecent: 'save as pdf recent',
    GoogleDriveRecent: 'google drive recent',
    SelectSaveAsPdf: 'select save as pdf',
    SelectGoogleDrive: 'select google drive',
    SelectRecentDestination: 'select recent destination',
    OpenDialog: 'open dialog',
    TwoAccountsRecentDestinations: 'two accounts recent destinations',
    UpdateRecentDestinations: 'update recent destinations',
  };

  const suiteName = 'DestinationSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationSettingsElement} */
    let destinationSettings = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?print_preview.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {!Array<!print_preview.RecentDestination>} */
    let recentDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {string} */
    const defaultUser = 'foo@chromium.org';

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      PolymerTest.clearBody();

      // Stub out native layer and cloud print interface.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      let localDestinations = [];
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      nativeLayer.setLocalDestinations(localDestinations);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();

      destinationSettings =
          document.createElement('print-preview-destination-settings');
      destinationSettings.settings = {
        recentDestinations: {
          value: [],
          unavailableValue: [],
          available: true,
          valid: true,
          setByPolicy: false,
          key: 'recentDestinations',
        },
      };
      destinationSettings.disabled = true;
      document.body.appendChild(destinationSettings);
    });

    // Tests that the dropdown is enabled or disabled correctly based on
    // the state.
    test(assert(TestNames.ChangeDropdownState), function() {
      const dropdown = destinationSettings.$.destinationSelect;
      // Initial state: No destination store means that there is no destination
      // yet, so the dropdown is hidden.
      assertTrue(dropdown.hidden);

      // Set up the destination store, but no destination yet. Dropdown is still
      // hidden.
      destinationSettings.initDestinationStore(
          'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */);
      assertTrue(dropdown.hidden);

      return test_util
          .eventToPromise(
              print_preview.DestinationStore.EventType
                  .SELECTED_DESTINATION_CAPABILITIES_READY,
              destinationSettings.destinationStore_)
          .then(() => {
            // Dropdown is visible but disabled since controls are disabled.
            assertTrue(dropdown.disabled);
            assertFalse(dropdown.hidden);
            destinationSettings.disabled = false;

            assertFalse(dropdown.disabled);

            // Simulate setting a setting to an invalid value. Dropdown is
            // disabled due to validation error on another control.
            destinationSettings.disabled = true;
            assertTrue(dropdown.disabled);

            // Simulate the user fixing the validation error, and then selecting
            // an invalid printer. Dropdown is enabled, so that the user can fix
            // the error.
            destinationSettings.disabled = false;
            destinationSettings.destinationStore_.dispatchEvent(new CustomEvent(
                print_preview.DestinationStore.EventType.ERROR,
                {detail: print_preview.DestinationErrorType.INVALID}));
            Polymer.dom.flush();

            assertEquals(
                print_preview.DestinationState.INVALID,
                destinationSettings.destinationState);
            destinationSettings.disabled = true;
            assertFalse(dropdown.disabled);

            if (cr.isChromeOS) {
              // Simulate the user having no printers.
              destinationSettings.destinationStore_.dispatchEvent(
                  new CustomEvent(
                      print_preview.DestinationStore.EventType.ERROR, {
                        detail:
                            print_preview.DestinationErrorType.NO_DESTINATIONS
                      }));
              Polymer.dom.flush();

              assertEquals(
                  print_preview.DestinationState.NO_DESTINATIONS,
                  destinationSettings.destinationState);
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);
            }
          });
    });

    /** @return {!print_preview.DestinationOrigin} */
    function getLocalOrigin() {
      return cr.isChromeOS ? print_preview.DestinationOrigin.CROS :
                             print_preview.DestinationOrigin.LOCAL;
    }

    /**
     * Initializes the destination store and destination settings using
     * |destinations| and |recentDestinations|.
     */
    function initialize() {
      // Initialize destination settings.
      destinationSettings.setCloudPrintInterface(cloudPrintInterface);
      destinationSettings.setSetting('recentDestinations', recentDestinations);
      destinationSettings.appKioskMode = false;
      destinationSettings.initDestinationStore(
          '' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */);
      destinationSettings.disabled = false;
    }

    /** Simulates a user signing in to Chrome. */
    function signIn() {
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(defaultUser));
      cr.webUIListenerCallback('reload-printer-list');
      Polymer.dom.flush();
    }

    /**
     * @param {string} id The id of the local destination.
     * @return {string} The key corresponding to the local destination, with the
     *     origin set correctly based on the platform.
     */
    function makeLocalDestinationKey(id) {
      return id + '/' + getLocalOrigin() + '/';
    }

    /**
     * @param {!Array<string>} expectedDestinations An array of the expected
     *     destinations in the dropdown.
     */
    function assertDropdownItems(expectedDestinations) {
      let options =
          destinationSettings.$.destinationSelect.shadowRoot.querySelectorAll(
              'option:not([hidden])');
      // assertEquals(expectedDestinations.length + 1, options.length);
      expectedDestinations.forEach((expectedValue, index) => {
        assertEquals(expectedValue, options[index].value);
      });
      assertEquals('seeMore', options[expectedDestinations.length].value);
    }

    // Tests that the dropdown contains the appropriate destinations when there
    // are no recent destinations.
    test(assert(TestNames.NoRecentDestinations), function() {
      initialize();
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the Save as PDF
            // destination.
            destinationSettings.destination =
                print_preview_test_utils.getSaveAsPdfDestination();
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems(['Save as PDF/local/']);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when there
    // are 3 recent destinations.
    test(assert(TestNames.RecentDestinations), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialize();

      // Wait for the destinations to be inserted into the store.
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when Save
    // as PDF is one of the recent destinations.
    test(assert(TestNames.SaveAsPdfRecent), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getSaveAsPdfDestination()));
      initialize();

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when
    // Google Drive is in the recent destinations.
    test(assert(TestNames.GoogleDriveRecent), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getGoogleDriveDestination(defaultUser)));
      initialize();

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            // Google Drive does not show up even though it is recent, since the
            // user is not signed in and the destination is not available.
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that selecting the Save as PDF destination results in the
    // DESTINATION_SELECT event firing, with Save as PDF set as the current
    // destination.
    test(assert(TestNames.SelectSaveAsPdf), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getSaveAsPdfDestination()));
      initialize();

      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(dropdown.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);
            // Most recent destination is selected by default.
            assertEquals('ID1', destinationSettings.destination.id);

            // Simulate selection of Save as PDF printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire('selected-option-change', 'Save as PDF/local/');

            // Ensure this fires the destination select event.
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationSettings.destination.id);
          });
    });

    // Tests that selecting the Google Drive destination results in the
    // DESTINATION_SELECT event firing, with Google Drive set as the current
    // destination.
    test(assert(TestNames.SelectGoogleDrive), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getGoogleDriveDestination(defaultUser)));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(dropdown.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
            // Most recent destination is selected by default.
            assertEquals('ID1', destinationSettings.destination.id);

            // Simulate selection of Google Drive printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire(
                'selected-option-change',
                '__google__docs/cookies/foo@chromium.org');
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                destinationSettings.destination.id);
          });
    });

    // Tests that selecting a recent destination results in the
    // DESTINATION_SELECT event firing, with the recent destination set as the
    // current destination.
    test(assert(TestNames.SelectRecentDestination), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(dropdown.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);
            // Most recent destination is selected by default.
            assertEquals('ID1', destinationSettings.destination.id);

            // Simulate selection of Save as PDF printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire(
                'selected-option-change', makeLocalDestinationKey('ID2'));
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals('ID2', destinationSettings.destination.id);
          });
    });

    // Tests that selecting the 'see more' option opens the dialog.
    test(assert(TestNames.OpenDialog), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = destinations[0];
            assertFalse(dropdown.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            dropdown.fire('selected-option-change', 'seeMore');
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertTrue(
                destinationSettings.$$('print-preview-destination-dialog')
                    .isOpen());
          });
    });

    test(assert(TestNames.TwoAccountsRecentDestinations), function() {
      const account2 = 'bar@chromium.org';
      const driveUser1 =
          print_preview_test_utils.getGoogleDriveDestination(defaultUser);
      const driveUser2 =
          print_preview_test_utils.getGoogleDriveDestination(account2);
      const cloudPrinterUser1 = new print_preview.Destination(
          'FooCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: defaultUser});
      const cloudPrinterUser2 = new print_preview.Destination(
          'BarCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'BarCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: account2});
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(defaultUser));
      cloudPrintInterface.setPrinter(driveUser2);
      cloudPrintInterface.setPrinter(cloudPrinterUser1);
      cloudPrintInterface.setPrinter(cloudPrinterUser2);

      recentDestinations = [
        cloudPrinterUser1, cloudPrinterUser2, destinations[0]
      ].map(destination => print_preview.makeRecentDestination(destination));

      initialize();
      Polymer.dom.flush();

      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            destinationSettings.destination = cloudPrinterUser1;
            Polymer.dom.flush();
            assertFalse(dropdown.disabled);
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              'FooCloud/cookies/foo@chromium.org',
              makeLocalDestinationKey('ID1'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);

            dropdown.fire('selected-option-change', 'seeMore');
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            const dialog =
                destinationSettings.$$('print-preview-destination-dialog');
            assertTrue(dialog.isOpen());
            // Simulate setting a new account.
            dialog.fire('account-change', account2);
            Polymer.dom.flush();
            return test_util.waitForRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              'BarCloud/cookies/bar@chromium.org',
              makeLocalDestinationKey('ID1'),
              'Save as PDF/local/',
              '__google__docs/cookies/bar@chromium.org',
            ]);
          });
    });

    /**
     * @param {!Array<string>} expectedDestinationIds An array of the expected
     *     recent destination ids.
     */
    function assertRecentDestinations(expectedDestinationIds) {
      const recentDestinations =
          destinationSettings.getSettingValue('recentDestinations');
      assertEquals(expectedDestinationIds.length, recentDestinations.length);
      expectedDestinationIds.forEach((expectedId, index) => {
        assertEquals(expectedId, recentDestinations[index].id);
      });
    }

    function selectDestination(destination) {
      destinationSettings.destinationStore_.selectDestination(destination);
      Polymer.dom.flush();
    }

    /**
     * Tests that the destination being set correctly updates the recent
     * destinations array.
     */
    test(assert(TestNames.UpdateRecentDestinations), function() {
      // Recent destinations start out empty.
      assertRecentDestinations([]);

      initialize();

      // Recent destinations start out empty.
      assertRecentDestinations(['Save as PDF']);

      // Simulate setting a destination.
      selectDestination(destinations[0]);
      assertRecentDestinations(['ID1', 'Save as PDF']);

      // Reselect a recent destination. Still 2 destinations, but in a
      // different order.
      selectDestination(
          destinationSettings.destinationStore_.getDestinationByKey(
              'Save as PDF/local/')),
          assertRecentDestinations(['Save as PDF', 'ID1']);

      // Select a third destination
      selectDestination(destinations[1]);
      assertRecentDestinations(['ID2', 'Save as PDF', 'ID1']);

      // Select a fourth destination. List does not grow.
      selectDestination(destinations[2]);
      assertRecentDestinations(['ID3', 'ID2', 'Save as PDF']);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
