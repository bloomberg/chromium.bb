// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('preview_generation_test', function() {
  /** @enum {string} */
  const TestNames = {
    Color: 'color',
    CssBackground: 'css background',
    FitToPage: 'fit to page',
    HeaderFooter: 'header/footer',
    Layout: 'layout',
    Margins: 'margins',
    CustomMargins: 'custom margins',
    MediaSize: 'media size',
    PageRange: 'page range',
    Rasterize: 'rasterize',
    PagesPerSheet: 'pages per sheet',
    Scaling: 'scaling',
    SelectionOnly: 'selection only',
    Destination: 'destination',
    ChangeMarginsByPagesPerSheet: 'change margins by pages per sheet',
  };

  const suiteName = 'PreviewGenerationTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {!print_preview.NativeInitialSettings} */
    const initialSettings =
        print_preview_test_utils.getDefaultInitialSettings();

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
    });

    /**
     * Initializes the UI with a default local destination and a 3 page document
     * length.
     * @return {!Promise} Promise that resolves when initialization is done,
     *     destination is set, and initial preview request is complete.
     */
    function initialize() {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview.PluginProxy.setInstance(pluginProxy);

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));

      return Promise
          .all([
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities'),
          ])
          .then(function() {
            const documentInfo = page.$$('print-preview-document-info');
            if (!documentInfo.documentSettings.isModifiable) {
              documentInfo.documentSettings.fitToPageScaling = 98;
            }
            documentInfo.documentSettings.pageCount = 3;
            documentInfo.margins = new print_preview.Margins(10, 10, 10, 10);
            return nativeLayer.whenCalled('getPreview');
          });
    }

    /**
     * @param {string} settingName The name of the setting to test.
     * @param {boolean | string} initialSettingValue The default setting value.
     * @param {boolean | string} updatedSettingValue The setting value to update
     *     to.
     * @param {string} ticketKey The field in the print ticket that corresponds
     *     to the setting.
     * @param {boolean | string | number} initialTicketValue The ticket value
     *     corresponding to the default setting value.
     * @param {boolean | string | number} updatedTicketValue The ticket value
     *     corresponding to the updated setting value.
     * @return {!Promise} Promise that resolves when the setting has been
     *     changed, the preview has been regenerated, and the print ticket and
     *     UI state have been verified.
     */
    function testSimpleSetting(
        settingName, initialSettingValue, updatedSettingValue, ticketKey,
        initialTicketValue, updatedTicketValue) {
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            assertEquals(0, originalTicket.requestID);
            assertEquals(initialTicketValue, originalTicket[ticketKey]);
            nativeLayer.resetResolver('getPreview');
            assertEquals(
                initialSettingValue, page.getSettingValue(settingName));
            page.setSetting(settingName, updatedSettingValue);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            assertEquals(
                updatedSettingValue, page.getSettingValue(settingName));
            const ticket = JSON.parse(args.printTicket);
            assertEquals(updatedTicketValue, ticket[ticketKey]);
            assertEquals(1, ticket.requestID);
          });
    }

    /** Validate changing the color updates the preview. */
    test(assert(TestNames.Color), function() {
      return testSimpleSetting(
          'color', true, false, 'color', print_preview.ColorMode.COLOR,
          print_preview.ColorMode.GRAY);
    });

    /** Validate changing the background setting updates the preview. */
    test(assert(TestNames.CssBackground), function() {
      return testSimpleSetting(
          'cssBackground', false, true, 'shouldPrintBackgrounds', false, true);
    });

    /** Validate changing the fit to page setting updates the preview. */
    test(assert(TestNames.FitToPage), function() {
      // Set PDF document so setting is available.
      initialSettings.previewModifiable = false;
      return testSimpleSetting(
          'fitToPage', false, true, 'fitToPageEnabled', false, true);
    });

    /** Validate changing the header/footer setting updates the preview. */
    test(assert(TestNames.HeaderFooter), function() {
      return testSimpleSetting(
          'headerFooter', true, false, 'headerFooterEnabled', true, false);
    });

    /** Validate changing the orientation updates the preview. */
    test(assert(TestNames.Layout), function() {
      return testSimpleSetting('layout', false, true, 'landscape', false, true);
    });

    /** Validate changing the margins updates the preview. */
    test(assert(TestNames.Margins), function() {
      return testSimpleSetting(
          'margins', print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          print_preview.ticket_items.MarginsTypeValue.MINIMUM, 'marginsType',
          print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          print_preview.ticket_items.MarginsTypeValue.MINIMUM);
    });

    /**
     * Validate changing the custom margins updates the preview, only after all
     * values have been set.
     */
    test(assert(TestNames.CustomMargins), function() {
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            assertEquals(
                print_preview.ticket_items.MarginsTypeValue.DEFAULT,
                originalTicket.marginsType);
            // Custom margins should not be set in the ticket.
            assertEquals(undefined, originalTicket.marginsCustom);
            assertEquals(0, originalTicket.requestID);

            // This should do nothing.
            page.setSetting(
                'margins', print_preview.ticket_items.MarginsTypeValue.CUSTOM);
            // Sets only 1 side, not valid.
            page.setSetting('customMargins', {marginTop: 25});
            // 2 sides, still not valid.
            page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
            // This should trigger a preview.
            nativeLayer.resetResolver('getPreview');
            page.setSetting('customMargins', {
              marginTop: 25,
              marginRight: 40,
              marginBottom: 20,
              marginLeft: 50
            });
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(
                print_preview.ticket_items.MarginsTypeValue.CUSTOM,
                ticket.marginsType);
            assertEquals(25, ticket.marginsCustom.marginTop);
            assertEquals(40, ticket.marginsCustom.marginRight);
            assertEquals(20, ticket.marginsCustom.marginBottom);
            assertEquals(50, ticket.marginsCustom.marginLeft);
            assertEquals(1, ticket.requestID);
            page.setSetting(
                'margins', print_preview.ticket_items.MarginsTypeValue.DEFAULT);
            // Set setting to something invalid and then set margins to CUSTOM.
            page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
            page.setSetting(
                'margins', print_preview.ticket_items.MarginsTypeValue.CUSTOM);
            nativeLayer.resetResolver('getPreview');
            page.setSetting('customMargins', {
              marginTop: 25,
              marginRight: 40,
              marginBottom: 20,
              marginLeft: 50
            });
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(
                print_preview.ticket_items.MarginsTypeValue.CUSTOM,
                ticket.marginsType);
            assertEquals(25, ticket.marginsCustom.marginTop);
            assertEquals(40, ticket.marginsCustom.marginRight);
            assertEquals(20, ticket.marginsCustom.marginBottom);
            assertEquals(50, ticket.marginsCustom.marginLeft);
            // Request 3. Changing to default margins should have triggered a
            // preview, and the final setting of valid custom margins should
            // have triggered another one.
            assertEquals(3, ticket.requestID);
          });
    });

    /**
     * Validate changing the pages per sheet updates the preview, and resets
     * margins to print_preview.ticket_items.MarginsTypeValue.DEFAULT.
     */
    test(assert(TestNames.ChangeMarginsByPagesPerSheet), function() {
      const marginsTypeEnum = print_preview.ticket_items.MarginsTypeValue;
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            assertEquals(0, originalTicket.requestID);
            assertEquals(
                marginsTypeEnum.DEFAULT, originalTicket['marginsType']);
            assertEquals(
                marginsTypeEnum.DEFAULT, page.getSettingValue('margins'));
            assertEquals(1, page.getSettingValue('pagesPerSheet'));
            assertEquals(1, originalTicket['pagesPerSheet']);
            nativeLayer.resetResolver('getPreview');
            page.setSetting('margins', marginsTypeEnum.MINIMUM);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            assertEquals(
                marginsTypeEnum.MINIMUM, page.getSettingValue('margins'));
            const ticket = JSON.parse(args.printTicket);
            assertEquals(marginsTypeEnum.MINIMUM, ticket['marginsType']);
            nativeLayer.resetResolver('getPreview');
            assertEquals(1, ticket.requestID);
            page.setSetting('pagesPerSheet', 4);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            assertEquals(
                marginsTypeEnum.DEFAULT, page.getSettingValue('margins'));
            assertEquals(4, page.getSettingValue('pagesPerSheet'));
            const ticket = JSON.parse(args.printTicket);
            assertEquals(marginsTypeEnum.DEFAULT, ticket['marginsType']);
            assertEquals(4, ticket['pagesPerSheet']);
            assertEquals(2, ticket.requestID);
          });
    });

    /** Validate changing the paper size updates the preview. */
    test(assert(TestNames.MediaSize), function() {
      const mediaSizeCapability =
          print_preview_test_utils.getCddTemplate('FooDevice')
              .capabilities.printer.media_size;
      const letterOption = mediaSizeCapability.option[0];
      const squareOption = mediaSizeCapability.option[1];
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            assertEquals(
                letterOption.width_microns,
                originalTicket.mediaSize.width_microns);
            assertEquals(
                letterOption.height_microns,
                originalTicket.mediaSize.height_microns);
            assertEquals(0, originalTicket.requestID);
            nativeLayer.resetResolver('getPreview');
            const mediaSizeSetting = page.getSettingValue('mediaSize');
            assertEquals(
                letterOption.width_microns, mediaSizeSetting.width_microns);
            assertEquals(
                letterOption.height_microns, mediaSizeSetting.height_microns);
            page.setSetting('mediaSize', squareOption);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const mediaSizeSettingUpdated = page.getSettingValue('mediaSize');
            assertEquals(
                squareOption.width_microns,
                mediaSizeSettingUpdated.width_microns);
            assertEquals(
                squareOption.height_microns,
                mediaSizeSettingUpdated.height_microns);
            const ticket = JSON.parse(args.printTicket);
            assertEquals(
                squareOption.width_microns, ticket.mediaSize.width_microns);
            assertEquals(
                squareOption.height_microns, ticket.mediaSize.height_microns);
            nativeLayer.resetResolver('getPreview');
            assertEquals(1, ticket.requestID);
          });
    });

    /** Validate changing the page range updates the preview. */
    test(assert(TestNames.PageRange), function() {
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            // Ranges is empty for full document.
            assertEquals(0, page.getSettingValue('ranges').length);
            assertEquals(0, originalTicket.pageRange.length);
            nativeLayer.resetResolver('getPreview');
            page.setSetting('ranges', [{from: 1, to: 2}]);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const setting = page.getSettingValue('ranges');
            assertEquals(1, setting.length);
            assertEquals(1, setting[0].from);
            assertEquals(2, setting[0].to);
            const ticket = JSON.parse(args.printTicket);
            assertEquals(1, ticket.pageRange.length);
            assertEquals(1, ticket.pageRange[0].from);
            assertEquals(2, ticket.pageRange[0].to);
          });
    });

    /** Validate changing the selection only setting updates the preview. */
    test(assert(TestNames.SelectionOnly), function() {
      // Set has selection to true so that the setting is available.
      initialSettings.hasSelection = true;
      return testSimpleSetting(
          'selectionOnly', false, true, 'shouldPrintSelectionOnly', false,
          true);
    });

    /** Validate changing the pages per sheet updates the preview. */
    test(assert(TestNames.PagesPerSheet), function() {
      return testSimpleSetting('pagesPerSheet', 1, 2, 'pagesPerSheet', 1, 2);
    });

    /** Validate changing the scaling updates the preview. */
    test(assert(TestNames.Scaling), function() {
      return initialize()
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(0, ticket.requestID);
            assertEquals(100, ticket.scaleFactor);
            nativeLayer.resetResolver('getPreview');
            assertEquals('100', page.getSettingValue('scaling'));
            assertEquals(false, page.getSettingValue('customScaling'));
            page.setSetting('customScaling', true);
            // Need to set custom value != 100 for preview to regenerate.
            page.setSetting('scaling', '90');
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(90, ticket.scaleFactor);
            assertEquals(1, ticket.requestID);
            nativeLayer.resetResolver('getPreview');
            assertEquals('90', page.getSettingValue('scaling'));
            assertEquals(true, page.getSettingValue('customScaling'));
            // This should regenerate the preview, since the custom value is not
            // 100.
            page.setSetting('customScaling', false);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(100, ticket.scaleFactor);
            assertEquals(2, ticket.requestID);
            nativeLayer.resetResolver('getPreview');
            assertEquals('90', page.getSettingValue('scaling'));
            assertEquals(false, page.getSettingValue('customScaling'));
            page.setSetting('customScaling', true);
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            // Back to custom 90.
            assertEquals(90, ticket.scaleFactor);
            assertEquals(3, ticket.requestID);
            nativeLayer.resetResolver('getPreview');
            assertEquals('90', page.getSettingValue('scaling'));
            assertEquals(true, page.getSettingValue('customScaling'));
            // When custom scaling is set, changing scaling updates preview.
            page.setSetting('scaling', '80');
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            const ticket = JSON.parse(args.printTicket);
            assertEquals(80, ticket.scaleFactor);
            assertEquals(4, ticket.requestID);
            assertEquals('80', page.getSettingValue('scaling'));
            assertEquals(true, page.getSettingValue('customScaling'));
          });
    });

    /**
     * Validate changing the rasterize setting updates the preview. Only runs
     * on Linux and CrOS as setting is not available on other platforms.
     */
    test(assert(TestNames.Rasterize), function() {
      // Set PDF document so setting is available.
      initialSettings.previewModifiable = false;
      return testSimpleSetting(
          'rasterize', false, true, 'rasterizePDF', false, true);
    });

    /**
     * Validate changing the destination updates the preview, if it results
     * in a settings change.
     */
    test(assert(TestNames.Destination), function() {
      return initialize()
          .then(function(args) {
            const originalTicket = JSON.parse(args.printTicket);
            assertEquals('FooDevice', page.destination_.id);
            assertEquals('FooDevice', originalTicket.deviceName);
            const barDestination = new print_preview.Destination(
                'BarDevice', print_preview.DestinationType.LOCAL,
                print_preview.DestinationOrigin.LOCAL, 'BarName',
                print_preview.DestinationConnectionStatus.ONLINE);
            const capabilities =
                print_preview_test_utils.getCddTemplate(barDestination.id)
                    .capabilities;
            capabilities.printer.media_size = {
              option: [
                {
                  name: 'ISO_A4',
                  width_microns: 210000,
                  height_microns: 297000,
                  custom_display_name: 'A4',
                },
              ],
            };
            barDestination.capabilities = capabilities;
            nativeLayer.resetResolver('getPreview');
            page.destinationState_ = print_preview.DestinationState.SELECTED;
            page.set('destination_', barDestination);
            page.destinationState_ = print_preview.DestinationState.UPDATED;
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(args) {
            assertEquals('BarDevice', page.destination_.id);
            const ticket = JSON.parse(args.printTicket);
            assertEquals('BarDevice', ticket.deviceName);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
