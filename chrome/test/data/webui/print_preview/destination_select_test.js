// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_select_test', function() {
  /** @enum {string} */
  const TestNames = {
    SingleRecentDestination: 'single recent destination',
    MultipleRecentDestinations: 'multiple recent destinations',
    MultipleRecentDestinationsOneRequest:
        'multiple recent destinations one request',
    DefaultDestinationSelectionRules: 'default destination selection rules',
    SystemDefaultPrinterPolicy: 'system default printer policy',
    KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
    NoPrintersShowsError: 'no printers shows error',
    UnreachableRecentCloudPrinter: 'unreachable recent cloud printer',
    RecentSaveAsPdf: 'recent save as pdf',
    MultipleRecentDestinationsAccounts: 'multiple recent destinations accounts',
  };

  const suiteName = 'DestinationSelectTests';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?PrintPreview.NativeLayerStub} */
    let nativeLayer = null;

    /** @type {?print_preview.NativeInitialSettngs} */
    let initialSettings = null;

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let cloudDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {number} */
    let numPrintersSelected = 0;

    /** @override */
    setup(function() {
      initialSettings = print_preview_test_utils.getDefaultInitialSettings();
      nativeLayer = new print_preview.NativeLayerStub();
      localDestinations = [];
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
    });

    /*
     * Sets the initial settings to the stored value and creates the page.
     * @param {boolean=} opt_expectPrinterFailure Whether printer fetch is
     *     expected to fail
     * @return {!Promise} Promise that resolves when initial settings and,
     *     if printer failure is not expected, printer capabilities have
     *     been returned.
     */
    function setInitialSettings(opt_expectPrinterFailure) {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinations(localDestinations);
      print_preview.NativeLayer.setInstance(nativeLayer);
      const cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      cloudprint.setCloudPrintInterfaceForTesting(cloudPrintInterface);
      cloudDestinations.forEach(cloudDestination => {
        cloudPrintInterface.setPrinter(cloudDestination);
      });
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      cr.webUIListenerCallback('use-cloud-print', 'cloudprint url', false);
      page.destinationStore_.addEventListener(
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          function() {
            numPrintersSelected++;
          });
      const whenCapabilitiesReady = test_util.eventToPromise(
          print_preview.DestinationStore.EventType
              .SELECTED_DESTINATION_CAPABILITIES_READY,
          page.destinationStore_);

      return nativeLayer.whenCalled('getInitialSettings').then(() => {
        return opt_expectPrinterFailure ? Promise.resolve() : Promise.race([
          nativeLayer.whenCalled('getPrinterCapabilities'),
          whenCapabilitiesReady
        ]);
      });
    }

    /**
     * Checks that a printer is displayed to the user with the name given
     * by |printerName|.
     * @param {string} printerName The printer name that should be displayed.
     * @param {boolean} disabled Whether the dropdown should be disabled.
     * @return {!Promise} Promise that resolves when checks are complete.
     */
    function assertPrinterDisplay(printerName, disabled) {
      const destinationSettings = page.$$('print-preview-destination-settings');
      const destinationSelect = destinationSettings.$.destinationSelect;

      Polymer.dom.flush();
      return test_util.waitForRender(destinationSelect, () => {
        // Check that the throbber is hidden and the dropdown is shown.
        assertTrue(destinationSettings.$$('.throbber-container').hidden);
        assertFalse(destinationSelect.hidden);
        assertEquals(disabled, destinationSelect.disabled);

        const options = destinationSelect.shadowRoot.querySelectorAll('option');
        const selectedOption =
            options[destinationSelect.$$('.md-select').selectedIndex];
        // Check that the destination matches the expected destination.
        assertEquals(printerName, selectedOption.textContent.trim());
      });
    }

    /**
     * Tests that if the user has a single valid recent destination the
     * destination is automatically reselected.
     */
    test(assert(TestNames.SingleRecentDestination), function() {
      const recentDestination =
          print_preview.makeRecentDestination(destinations[0]);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      return setInitialSettings().then(function(args) {
        assertEquals('ID1', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID1', page.destination_.id);
        return assertPrinterDisplay('One', false);
      });
    });

    /**
     * Tests that if the user has multiple valid recent destination the most
     * recent destination is automatically reselected and the remaining
     * destinations are prefetched.
     */
    test(assert(TestNames.MultipleRecentDestinations), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings()
          .then(function(args) {
            // Should have loaded ID1 as the selected printer, since it was most
            // recent.
            assertEquals('ID1', args.destinationId);
            assertEquals(print_preview.PrinterType.LOCAL, args.type);
            assertEquals('ID1', page.destination_.id);
            return assertPrinterDisplay('One', false);
          })
          .then(function() {
            // Verify the correct printers are marked as recent in the store.
            const reportedPrinters = page.destinationStore_.destinations();
            destinations.forEach((destination, index) => {
              const match = reportedPrinters.find((reportedPrinter) => {
                return reportedPrinter.id == destination.id;
              });
              assertEquals(index >= 3, typeof match === 'undefined');
            });
          });
    });

    /**
     * Tests that if the user has multiple valid recent destinations, this
     * does not result in multiple calls to getPrinterCapabilities and the
     * correct destination is selected for the preview request.
     * For crbug.com/666595.
     */
    test(assert(TestNames.MultipleRecentDestinationsOneRequest), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings()
          .then(function(args) {
            // Should have loaded ID1 as the selected printer, since it was most
            // recent.
            assertEquals('ID1', args.destinationId);
            assertEquals(print_preview.PrinterType.LOCAL, args.type);
            assertEquals('ID1', page.destination_.id);

            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(previewArgs) {
            const ticket = JSON.parse(previewArgs.printTicket);
            assertEquals(0, ticket.requestID);
            assertEquals('ID1', ticket.deviceName);

            // The other recent destinations should be prefetched, but only one
            // should have been selected so there was only one preview request.
            const reportedPrinters = page.destinationStore_.destinations();
            assertEquals(4, reportedPrinters.length);
            destinations.forEach((destination, index) => {
              assertEquals(
                  index < 3,
                  reportedPrinters.some(p => p.id == destination.id));
            });
            assertEquals(1, numPrintersSelected);
          });
    });

    /**
     * Tests that if there are default destination selection rules they are
     * respected and a matching destination is automatically selected.
     */
    test(assert(TestNames.DefaultDestinationSelectionRules), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr =
          JSON.stringify({namePattern: '.*Four.*'});
      initialSettings.serializedAppStateStr = '';
      return setInitialSettings().then(function(args) {
        // Should have loaded ID4 as the selected printer, since it matches
        // the rules.
        assertEquals('ID4', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID4', page.destination_.id);
        return assertPrinterDisplay('Four', false);
      });
    });

    /**
     * Tests that if the system default printer policy is enabled the system
     * default printer is automatically selected even if the user has recent
     * destinations.
     */
    test(assert(TestNames.SystemDefaultPrinterPolicy), function() {
      // Setup some recent destinations to ensure they are not selected.
      const recentDestinations = [];
      destinations.slice(0, 3).forEach(destination => {
        recentDestinations.push(
            print_preview.makeRecentDestination(destination));
      });

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return Promise
          .all([
            setInitialSettings(),
            test_util.eventToPromise(
                print_preview.DestinationStore.EventType
                    .SELECTED_DESTINATION_CAPABILITIES_READY,
                page.destinationStore_),
          ])
          .then(function(argsArray) {
            // Need to load FooDevice as the printer, since it is the system
            // default.
            assertEquals('FooDevice', page.destination_.id);
            assertPrinterDisplay('FooName', false);
          });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, and the preview
     * is in app kiosk mode (so no PDF printer), the first destination returned
     * from printer fetch is selected.
     */
    test(assert(TestNames.KioskModeSelectsFirstPrinter), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';

      return setInitialSettings().then(function(args) {
        // Should have loaded the first destination as the selected printer.
        assertEquals(destinations[0].id, args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals(destinations[0].id, page.destination_.id);
        return assertPrinterDisplay(destinations[0].displayName, false);
      });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, the preview
     * is in app kiosk mode (so no PDF printer), and there are no
     * destinations found, the no destinations found error is displayed.
     */
    test(assert(TestNames.NoPrintersShowsError), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';
      localDestinations = [];

      return Promise
          .all([
            setInitialSettings(true),
            test_util.eventToPromise(
                print_preview.DestinationStore.EventType.NO_DESTINATIONS_FOUND,
                page.destinationStore_),
          ])
          .then(function() {
            assertEquals(null, page.destination_);
            const destinationSettings =
                page.$$('print-preview-destination-settings');
            assertTrue(destinationSettings.$$('.throbber-container').hidden);
            const destinationSelect = destinationSettings.$.destinationSelect;
            assertFalse(destinationSelect.hidden);
            const selected = destinationSelect.$$('option[selected]');
            assertEquals('noDestinations', selected.value);
          });
    });

    /**
     * Tests that if the user has a recent destination that triggers a cloud
     * print error this does not disable the dialog.
     */
    test(assert(TestNames.UnreachableRecentCloudPrinter), function() {
      const cloudPrinter =
          print_preview_test_utils.createDestinationWithCertificateStatus(
              'BarDevice', 'BarName', false);
      const recentDestination =
          print_preview.makeRecentDestination(cloudPrinter);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      return setInitialSettings().then(function(args) {
        assertEquals(print_preview_new.State.READY, page.state);
        assertEquals('FooDevice', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('FooDevice', page.destination_.id);
        return assertPrinterDisplay('FooName', false);
      });
    });

    /**
     * Tests that if the user has a recent destination that is already in the
     * store (PDF printer), the DestinationStore does not try to select a
     * printer again later. Regression test for https://crbug.com/927162.
     */
    test(assert(TestNames.RecentSaveAsPdf), function() {
      const pdfPrinter = print_preview_test_utils.getSaveAsPdfDestination();
      const recentDestination = print_preview.makeRecentDestination(pdfPrinter);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      print_preview.DestinationStore.AUTO_SELECT_TIMEOUT_ = 0;
      return setInitialSettings()
          .then(function() {
            assertEquals(print_preview_new.State.READY, page.state);
            assertPrinterDisplay('Save as PDF', false);
            // Simulate setting a bad ticket value.
            page.$.state.transitTo(print_preview_new.State.INVALID_TICKET);
            return new Promise(resolve => setTimeout(resolve));
          })
          .then(function() {
            // Should still have Save as PDF. Dropdown is disabled due to
            // invalid ticket.
            assertPrinterDisplay('Save as PDF', true);
            assertEquals(print_preview_new.State.INVALID_TICKET, page.state);
          });
    });

    /**
     * Tests that if there are recent destinations from different accounts, only
     * destinations associated with the most recent account are fetched.
     */
    test(assert(TestNames.MultipleRecentDestinationsAccounts), function() {
      const account1 = 'foo@chromium.org';
      const account2 = 'bar@chromium.org';
      const driveUser1 =
          print_preview_test_utils.getGoogleDriveDestination(account1);
      const driveUser2 =
          print_preview_test_utils.getGoogleDriveDestination(account2);
      const cloudPrinterUser1 = new print_preview.Destination(
          'FooCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: account1});
      const recentDestinations = [
        print_preview.makeRecentDestination(driveUser1),
        print_preview.makeRecentDestination(driveUser2),
        print_preview.makeRecentDestination(cloudPrinterUser1),
      ];
      cloudDestinations = [driveUser1, driveUser2, cloudPrinterUser1];
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings()
          .then(() => {
            // Should have loaded Google Drive as the selected printer, since it
            // was most recent.
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                page.destination_.id);
            assertPrinterDisplay('Save to Google Drive', false);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(previewArgs) {
            const ticket = JSON.parse(previewArgs.printTicket);
            assertEquals(0, ticket.requestID);
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                ticket.deviceName);

            // Only the other cloud destination for the same user account should
            // have been prefetched.
            const loadedPrinters = page.destinationStore_.destinations();
            assertEquals(3, loadedPrinters.length);
            cloudDestinations.forEach((destination) => {
              assertEquals(
                  destination.account === account1,
                  loadedPrinters.some(p => p.key == destination.key));
            });
            assertEquals(1, numPrintersSelected);
          });
    });

  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
