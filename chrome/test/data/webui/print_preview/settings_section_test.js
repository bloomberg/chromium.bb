// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Copies: 'copies',
    Layout: 'layout',
    Color: 'color',
    MediaSize: 'media size',
    Margins: 'margins',
    Dpi: 'dpi',
    Scaling: 'scaling',
    Other: 'other',
    SetCopies: 'set copies',
    SetLayout: 'set layout',
    SetColor: 'set color',
  };

  const suiteName = 'SettingsSectionsTests';
  suite(suiteName, function() {
    let page = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);

      const fooDestination = new print_preview.Destination(
          'FooPrinter', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'Foo Printer',
          false /*isRecent*/, print_preview.DestinationConnectionStatus.ONLINE);
      fooDestination.capabilities =
          print_preview_test_utils.getCddTemplate(fooDestination.id)
              .capabilities;
      page.set('destination_', fooDestination);
    });

    /** @param {boolean} isPdf Whether the document should be a PDF. */
    function setPdfDocument(isPdf) {
      const info = new print_preview.DocumentInfo();
      info.init(!isPdf, 'title', false);
      page.set('documentInfo_', info);
    }

    function setPdfDestination() {
      const saveAsPdfDestination = new print_preview.Destination(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL,
          loadTimeData.getString('printToPDF'), false /*isRecent*/,
          print_preview.DestinationConnectionStatus.ONLINE);
      saveAsPdfDestination.capabilities =
          print_preview_test_utils.getCddTemplate(saveAsPdfDestination.id)
              .capabilities;
      page.set('destination_', saveAsPdfDestination);
    }

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      expectEquals(false, copiesElement.hidden);

      // Remove copies capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.copies;

      // Copies section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, copiesElement.hidden);
    });

    test(assert(TestNames.Layout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');

      // Set up with HTML document.
      setPdfDocument(false);
      expectEquals(false, layoutElement.hidden);

      // Remove layout capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.page_orientation;

      // Each of these settings should not show the capability.
      [null, {option: [{type: 'PORTRAIT', is_default: true}]},
       {option: [{type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, layoutElement.hidden);

      // Test with PDF - should be hidden.
      setPdfDocument(true);
      expectEquals(true, layoutElement.hidden);
    });

    test(assert(TestNames.Color), function() {
      const colorElement = page.$$('print-preview-color-settings');
      expectEquals(false, colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      // Each of these settings should not show the capability.
      [null, {option: [{type: 'STANDARD_COLOR', is_default: true}]}, {
        option: [
          {type: 'STANDARD_COLOR', is_default: true}, {type: 'CUSTOM_COLOR'}
        ]
      },
       {
         option: [
           {type: 'STANDARD_MONOCHROME', is_default: true},
           {type: 'CUSTOM_MONOCHROME'}
         ]
       },
       {option: [{type: 'STANDARD_MONOCHROME', is_default: true}]},
      ].forEach(colorCap => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = colorCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, colorElement.hidden);
      });

      // Custom color and monochrome options should make the section visible.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.color =
        {option: [{ type: 'CUSTOM_COLOR', is_default: true },
                  { type: 'CUSTOM_MONOCHROME' }]};
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, colorElement.hidden);
    });

    test(assert(TestNames.MediaSize), function() {
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      expectEquals(false, mediaSizeElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, mediaSizeElement.hidden);

      // Reset
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      // Set PDF document type.
      setPdfDocument(true);
      expectEquals(false, mediaSizeElement.hidden);

      // Set save as PDF. This should hide the settings section.
      setPdfDestination();
      expectEquals(true, mediaSizeElement.hidden);

      // Set HTML document type, should now show the section.
      setPdfDocument(false);
      expectEquals(false, mediaSizeElement.hidden);
    });

    test(assert(TestNames.Margins), function() {
      const marginsElement = page.$$('print-preview-margins-settings');

      // Section is available for HTML (modifiable) documents
      setPdfDocument(false);
      expectEquals(false, marginsElement.hidden);

      // Unavailable for PDFs.
      setPdfDocument(true);
      expectEquals(true, marginsElement.hidden);
    });

    test(assert(TestNames.Dpi), function() {
      const dpiElement = page.$$('print-preview-dpi-settings');
      expectEquals(false, dpiElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, dpiElement.hidden);

      // Does not show up for only 1 option.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.dpi.option.pop();
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, dpiElement.hidden);
    });

    test(assert(TestNames.Scaling), function() {
      const scalingElement = page.$$('print-preview-scaling-settings');
      expectEquals(false, scalingElement.hidden);

      // HTML to non-PDF destination -> only input shown
      setPdfDocument(false);
      const fitToPageContainer = scalingElement.$$('#fit-to-page-container');
      const scalingInput =
          scalingElement.$$('print-preview-number-settings-section')
              .$$('.user-value');
      expectEquals(false, scalingElement.hidden);
      expectEquals(true, fitToPageContainer.hidden);
      expectEquals(false, scalingInput.hidden);

      // PDF to non-PDF destination -> checkbox and input shown.
      setPdfDocument(true);
      expectEquals(false, scalingElement.hidden);
      expectEquals(false, fitToPageContainer.hidden);
      expectEquals(false, scalingInput.hidden);

      // PDF to PDF destination -> section disappears.
      setPdfDestination();
      expectEquals(true, scalingElement.hidden);
    });

    test(assert(TestNames.Other), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$$('#header-footer-container');
      const duplex = optionsElement.$$('#duplex-container');
      const cssBackground = optionsElement.$$('#css-background-container');
      const rasterize = optionsElement.$$('#rasterize-container');
      const selectionOnly = optionsElement.$$('#selection-only-container');

      // Start with HTML + duplex capability.
      setPdfDocument(false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, headerFooter.hidden);
      expectEquals(false, duplex.hidden);
      expectEquals(false, cssBackground.hidden);
      expectEquals(true, rasterize.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add a selection.
      let info = new print_preview.DocumentInfo();
      info.init(true, 'title', true);
      page.set('documentInfo_', info);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, selectionOnly.hidden);

      // Remove duplex capability.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, optionsElement.hidden);
      expectEquals(true, duplex.hidden);

      // PDF
      setPdfDocument(true);
      expectEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      expectEquals(true, headerFooter.hidden);
      expectEquals(true, duplex.hidden);
      expectEquals(true, cssBackground.hidden);
      expectEquals(cr.isWindows || cr.isMac, rasterize.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add a selection - should do nothing for PDFs.
      info = new print_preview.DocumentInfo();
      info.init(false, 'title', true);
      page.set('documentInfo_', info);
      expectEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, duplex.hidden);
    });

    test(assert(TestNames.SetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      expectEquals(false, copiesElement.hidden);

      // Default value is 1
      const copiesInput =
          copiesElement.$$('print-preview-number-settings-section')
              .$$('.user-value');
      expectEquals('1', copiesInput.value);
      expectEquals(1, page.settings.copies.value);

      // Change to 2
      copiesInput.value = '2';
      copiesInput.dispatchEvent(new CustomEvent('input'));
      expectEquals(2, page.settings.copies.value);

      // Collate is true by default.
      const collateInput = copiesElement.$.collate;
      expectEquals(true, collateInput.checked);
      expectEquals(true, page.settings.collate.value);

      // Uncheck the box.
      MockInteractions.tap(collateInput);
      expectEquals(false, collateInput.checked);
      collateInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(false, page.settings.collate.value);
    });

    test(assert(TestNames.SetLayout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');
      expectEquals(false, layoutElement.hidden);

      // Default is portrait
      const layoutInput = layoutElement.$$('select');
      expectEquals('portrait', layoutInput.value);
      expectEquals(false, page.settings.layout.value);

      // Change to landscape
      layoutInput.value = 'landscape';
      layoutInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(true, page.settings.layout.value);
    });

    test(assert(TestNames.SetColor), function() {
      const colorElement = page.$$('print-preview-color-settings');
      expectEquals(false, colorElement.hidden);

      // Default is color
      const colorInput = colorElement.$$('select');
      expectEquals('color', colorInput.value);
      expectEquals(true, page.settings.color.value);

      // Change to black and white.
      colorInput.value = 'bw';
      colorInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(false, page.settings.color.value);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
