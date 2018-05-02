// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_select_test', function() {
  /** @enum {string} */
  const TestNames = {
    SingleRecentDestination: 'single recent destination',
    MultipleRecentDestinations: 'multiple recent destination',
    DefaultDestinationSelectionRules: 'default destination selection rules',
    SystemDefaultPrinterPolicy: 'system default printer policy',
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
    let destinations = [];

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
     * @return {!Promise} Promise that resolves when initial settings and
     *     printer capabilities have been returned.
     */
    function setInitialSettings() {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinations(localDestinations);
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);

      return Promise.all([
        nativeLayer.whenCalled('getInitialSettings'),
        nativeLayer.whenCalled('getPrinterCapabilities')
      ]);
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
        recentDestinations: [ recentDestination ],
      });

      return setInitialSettings().then(function(argsArray) {
        assertEquals('ID1', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('ID1', page.destination_.id);
      });
    });

    /**
     * Tests that if the user has multiple valid recent destination the most
     * recent destination is automatically reselected and the remaining
     * destinations are marked as recent in the store.
     */
    test(assert(TestNames.MultipleRecentDestinations), function() {
      const recentDestinations = [];
      destinations.slice(0, 3).forEach(destination => {
        recentDestinations.push(
            print_preview.makeRecentDestination(destination));
      });

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings().then(function(argsArray) {
        // Should have loaded ID1 as the selected printer, since it was most
        // recent.
        assertEquals('ID1', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('ID1', page.destination_.id);

        // Load all local destinations.
        page.destinationStore_.startLoadDestinations(
            print_preview.PrinterType.LOCAL_PRINTER);
        return nativeLayer.whenCalled('getPrinters');
      }).then(function() {
        // Verify the correct printers are marked as recent in the store.
        const reportedPrinters = page.destinationStore_.destinations();
        destinations.forEach((destination, index) => {
          const match = reportedPrinters.find((reportedPrinter) => {
              return reportedPrinter.id == destination.id;});
          assertFalse(typeof match === "undefined");
          if (index < 3)
            assertTrue(match.isRecent);
          else
            assertFalse(match.isRecent);
        });
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
      return setInitialSettings().then(function(argsArray) {
        // Should have loaded ID4 as the selected printer, since it matches
        // the rules.
        assertEquals('ID4', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('ID4', page.destination_.id);
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

      return setInitialSettings().then(function(argsArray) {
        // Need to load FooDevice as the printer, since it is the system
        // default.
        assertEquals('FooDevice', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('FooDevice', page.destination_.id);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
