// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_app_test', function() {
  /** @enum {string} */
  const TestNames = {
    PrintToGoogleDrive: 'print to google drive',
  };

  const suiteName = 'PrintPreviewAppTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?cloudprint.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {?print_preview.PluginProxy} */
    let pluginProxy = null;

    /** @type {!print_preview.NativeInitialSettings} */
    const initialSettings = {
      isInKioskAutoPrintMode: false,
      isInAppKioskMode: false,
      thousandsDelimeter: ',',
      decimalDelimeter: '.',
      unitType: 1,
      previewModifiable: true,
      documentTitle: 'DocumentABC123',
      documentHasSelection: false,
      shouldPrintSelectionOnly: false,
      printerName: 'FooDevice',
      serializedAppStateStr: null,
      serializedDefaultDestinationSelectionRulesStr: null
    };

    let localDestinations = [];
    let cloudDestinations = [];

    /** @override */
    setup(function() {
      // Stub out the native layer, the cloud print interface, and the plugin.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      cloudprint.setCloudPrintInterfaceForTesting(cloudPrintInterface);
      pluginProxy = new print_preview.PDFPluginStub();
      print_preview_new.PluginProxy.setInstance(pluginProxy);
    });

    /**
     * Initializes the native layer and cloud print interface based on
     * |initialSettings|, |localDestinations|, and |cloudDestinations|, and
     * creates the page.
     * @return {!Promise} Promise that resolves when the selected printer is
     *     loaded.
     */
    function finishSetup() {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinations(localDestinations);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      cloudDestinations.forEach(
          destination => cloudPrintInterface.setPrinter(destination));

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));
      cr.webUIListenerCallback('use-cloud-print', 'cloudprint url', false);

      return test_util.eventToPromise(
          print_preview.DestinationStore.EventType
              .SELECTED_DESTINATION_CAPABILITIES_READY,
          page.destinationStore_);
    }

    // Regression test for https://crbug.com/936029
    test(assert(TestNames.PrintToGoogleDrive), async () => {
      // Set up the UI to have Google Drive as the most recent printer.
      const account = 'foo@chromium.org';
      const drivePrinter =
          print_preview_test_utils.getGoogleDriveDestination(account);
      const recentDestinations = [
        print_preview.makeRecentDestination(drivePrinter),
      ];
      cloudDestinations = [drivePrinter];
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      await finishSetup();

      // Should have loaded Google Drive as the selected printer, since it
      // was most recent.
      assertEquals(
          print_preview.Destination.GooglePromotedId.DOCS,
          page.destination_.id);

      // Trigger print.
      const header = page.$$('print-preview-header');
      header.dispatchEvent(
          new CustomEvent('print-requested', {composed: true, bubbles: true}));

      // Validate arguments to cloud print interface.
      const args = await cloudPrintInterface.whenCalled('submit');
      assertEquals('sample data', args.data);
      assertEquals('DocumentABC123', args.documentTitle);
      assertEquals(
          print_preview.Destination.GooglePromotedId.DOCS, args.destination.id);
      assertEquals('1.0', JSON.parse(args.printTicket).version);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
