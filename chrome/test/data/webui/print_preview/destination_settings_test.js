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
      cloudPrintInterface.setPrinter(
          print_preview.Destination.GooglePromotedId.DOCS,
          print_preview_test_utils.getGoogleDriveDestination());

      destinationSettings =
          document.createElement('print-preview-destination-settings');
      destinationSettings.destinationStore = null;
      destinationSettings.recentDestinations = [];
      destinationSettings.state = print_preview_new.State.NOT_READY;
      // Disabled is true when state is NOT_READY.
      destinationSettings.disabled = true;
      document.body.appendChild(destinationSettings);
    });

    function waitForRender(element) {
      return new Promise(resolve => {
        if (Polymer.DomIf) {
          Polymer.RenderStatus.beforeNextRender(element, resolve);
        } else {
          setTimeout(() => {
            resolve();
          });
        }
      });
    }

    // Tests that the dropdown is enabled or disabled correctly based on
    // the state.
    test(assert(TestNames.ChangeDropdownState), function() {
      const dropdown = destinationSettings.$.destinationSelect;
      // Initial state: No destination store, dropdown should be disabled.
      assertTrue(dropdown.disabled);

      // Set up the destination store, but no destination yet. Dropdown is
      // disabled.
      const destinationStore =
          print_preview_test_utils.createDestinationStore();
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* recentDestinations */);
      destinationSettings.destinationStore = destinationStore;
      destinationSettings.state = print_preview_new.State.NOT_READY;
      assertTrue(dropdown.disabled);

      // Simulate loading a destination and setting state to ready. The dropdown
      // is enabled.
      destinationSettings.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL, getLocalOrigin(),
          'FooName', true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      destinationSettings.recentDestinations = [
        print_preview.makeRecentDestination(destinationSettings.destination),
      ];
      destinationSettings.state = print_preview_new.State.READY;
      destinationSettings.disabled = false;
      return waitForRender(dropdown).then(() => {
        assertFalse(dropdown.disabled);

        // Simulate setting a setting to an invalid value. Dropdown is disabled
        // due to validation error on another control.
        destinationSettings.state = print_preview_new.State.INVALID_TICKET;
        destinationSettings.disabled = true;
        assertTrue(dropdown.disabled);

        // Simulate the user fixing the validation error, and then selecting an
        // invalid printer. Dropdown is enabled, so that the user can fix the
        // error.
        destinationSettings.state = print_preview_new.State.READY;
        destinationSettings.disabled = false;
        destinationSettings.state = print_preview_new.State.INVALID_PRINTER;
        destinationSettings.disabled = true;
        assertFalse(dropdown.disabled);

        // Simulate the user having no printers.
        destinationSettings.destination = null;
        destinationSettings.state = print_preview_new.State.INVALID_PRINTER;
        destinationSettings.disabled = true;
        destinationSettings.noDestinationsFound = true;
        assertTrue(dropdown.disabled);
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
      // Initialize destination store.
      const destinationStore =
          print_preview_test_utils.createDestinationStore();
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          recentDestinations);
      destinationStore.setCloudPrintInterface(cloudPrintInterface);

      // Initialize destination settings.
      const defaultId = recentDestinations.length > 0 ?
          recentDestinations[0].id :
          'FooDevice';
      const defaultName = recentDestinations.length > 0 ?
          recentDestinations[0].displayName :
          'FooName';
      destinationSettings.destination = new print_preview.Destination(
          defaultId, print_preview.DestinationType.LOCAL, getLocalOrigin(),
          defaultName, true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      destinationSettings.destinationStore = destinationStore;
      destinationSettings.invitationStore = new print_preview.InvitationStore();
      destinationSettings.recentDestinations = recentDestinations;
      destinationSettings.appKioskMode = false;
      destinationSettings.activeUser = '';
      destinationSettings.users = [];
      destinationSettings.disabled = false;
      destinationSettings.state = print_preview_new.State.READY;
      destinationSettings.cloudPrintState =
          print_preview.CloudPrintState.ENABLED;
    }

    /** Simulates a user signing in to Chrome. */
    function signIn() {
      destinationSettings.activeUser = 'foo@chromium.org';
      destinationSettings.users = ['foo@chromium.org'];
      destinationSettings.cloudPrintState =
          print_preview.CloudPrintState.SIGNED_IN;
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
      assertEquals(expectedDestinations.length + 1, options.length);
      expectedDestinations.forEach((expectedValue, index) => {
        assertEquals(expectedValue, options[index].value);
      });
      assertEquals('seeMore', options[expectedDestinations.length].value);
    }

    // Tests that the dropdown contains the appropriate destinations when there
    // are no recent destinations.
    test(assert(TestNames.NoRecentDestinations), function() {
      initialize();
      assertFalse(destinationSettings.$.destinationSelect.disabled);
      assertDropdownItems(['Save as PDF/local/']);

      // If the user is signed in, Save to Drive should be displayed.
      signIn();
      return test_util.waitForRender(destinationSettings).then(() => {
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
      assertFalse(destinationSettings.$.destinationSelect.disabled);

      // Wait for the destinations to be inserted into the store.
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
      assertFalse(destinationSettings.$.destinationSelect.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
              print_preview_test_utils.getGoogleDriveDestination()));
      initialize();
      assertFalse(destinationSettings.$.destinationSelect.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
      assertFalse(dropdown.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
                destinationSettings.destinationStore);
            dropdown.fire('selected-option-change', 'Save as PDF/local/');

            // Ensure this fires the destination select event.
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationSettings.destinationStore.selectedDestination_.id);
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
              print_preview_test_utils.getGoogleDriveDestination()));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;
      assertFalse(dropdown.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
                destinationSettings.destinationStore);
            dropdown.fire(
                'selected-option-change',
                '__google__docs/cookies/foo@chromium.org');
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                destinationSettings.destinationStore.selectedDestination_.id);
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
      assertFalse(dropdown.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
                destinationSettings.destinationStore);
            dropdown.fire(
                'selected-option-change', makeLocalDestinationKey('ID2'));
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                'ID2',
                destinationSettings.destinationStore.selectedDestination_.id);
          });
    });

    // Tests that selecting the 'see more' option opens the dialog.
    test(assert(TestNames.OpenDialog), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;
      assertFalse(dropdown.disabled);

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
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
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
