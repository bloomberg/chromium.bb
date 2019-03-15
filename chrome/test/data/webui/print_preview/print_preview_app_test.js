// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_app_test', function() {
  /** @enum {string} */
  const TestNames = {
    PrintToGoogleDrive: 'print to google drive',
    SettingsSectionsVisibilityChange: 'settings sections visibility change',
    PrintPresets: 'print presets',
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

    /** @override */
    setup(function() {
      // Stub out the native layer, the cloud print interface, and the plugin.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      cloudprint.setCloudPrintInterfaceForTesting(cloudPrintInterface);
      pluginProxy = new print_preview.PDFPluginStub();
      print_preview_new.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));
      cr.webUIListenerCallback('use-cloud-print', 'cloudprint url', false);
      return nativeLayer.whenCalled('getInitialSettings').then(() => {
        page.destination_ = new print_preview.Destination(
            'FooDevice', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'FooName',
            print_preview.DestinationConnectionStatus.ONLINE);
        page.destination_.capabilities =
            print_preview_test_utils.getCddTemplate('FooDevice').capabilities;
      });
    });

    // Regression test for https://crbug.com/936029
    test(assert(TestNames.PrintToGoogleDrive), async () => {
      // Set up the UI to have Google Drive as the printer.
      page.destination_ = print_preview_test_utils.getGoogleDriveDestination(
          'foo@chromium.org');
      page.destination_.capabilities =
          print_preview_test_utils.getCddTemplate(page.destination_.id);

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

    test(assert(TestNames.SettingsSectionsVisibilityChange), function() {
      const moreSettingsElement = page.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
      const camelToKebab = s => s.replace(/([A-Z])/g, '-$1').toLowerCase();
      ['copies', 'layout', 'color', 'mediaSize', 'margins', 'dpi', 'scaling',
       'otherOptions']
          .forEach(setting => {
            const element =
                page.$$(`print-preview-${camelToKebab(setting)}-settings`);
            // Show, hide and reset.
            [true, false, true].forEach(value => {
              page.set(`settings.${setting}.available`, value);
              // Element expected to be visible when available.
              assertEquals(!value, element.hidden);
            });
          });
    });

    test(assert(TestNames.PrintPresets), function() {
      assertEquals(1, page.settings.copies.value);
      page.setSetting('duplex', false);
      assertFalse(page.settings.duplex.value);

      // Send preset values of duplex LONG_EDGE and 2 copies.
      const copies = 2;
      const duplex = print_preview_new.DuplexMode.LONG_EDGE;
      cr.webUIListenerCallback('print-preset-options', true, copies, duplex);
      assertEquals(copies, page.getSettingValue('copies'));
      assertTrue(page.getSettingValue('duplex'));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
