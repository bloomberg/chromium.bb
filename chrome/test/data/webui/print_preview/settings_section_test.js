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
  };

  const suiteName = 'SettingsSectionsTests';
  suite(suiteName, function() {
    let page = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
    });

    /** @return {!print_preview.Cdd} */
    function getCdd() {
      return {
        version: '1.0',
        printer: {
          collate: {default: true},
          copies: {default: 1, max: 1000},
          color: {
            option: [
              {type: 'STANDARD_COLOR', is_default: true},
              {type: 'STANDARD_MONOCHROME'}
            ]
          },
          dpi: {
            option: [
              {horizontal_dpi: 200, vertical_dpi: 200, is_default: true},
              {horizontal_dpi: 100, vertical_dpi: 100},
            ]
          },
          duplex: {
            option: [
              {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
              {type: 'SHORT_EDGE'}
            ]
          },
          page_orientation: {
            option: [
              {type: 'PORTRAIT', is_default: true}, {type: 'LANDSCAPE'},
              {type: 'AUTO'}
            ]
          },
          media_size: {
            option: [
              {
                name: 'NA_LETTER',
                width_microns: 215900,
                height_microns: 279400,
                is_default: true,
                custom_display_name: 'Letter',
              },
              {
                name: 'CUSTOM_SQUARE',
                width_microns: 215900,
                height_microns: 215900,
                custom_display_name: 'CUSTOM_SQUARE',
              }
            ]
          },
          vendor_capability: [],
        },
      };
    }

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
      saveAsPdfDestination.capabilities = getCdd();
      page.set('destination_', saveAsPdfDestination);
    }

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      expectEquals(false, copiesElement.hidden);

      // Remove copies capability.
      const capabilities = getCdd();
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
      let capabilities = getCdd();
      delete capabilities.printer.page_orientation;

      // Each of these settings should not show the capability.
      [
        null,
        {option: [{ type: 'PORTRAIT', is_default: true }]},
        {option: [{ type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        capabilities = getCdd();
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities = getCdd();
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
      let capabilities = getCdd();
      delete capabilities.printer.color;

      // Each of these settings should not show the capability.
      [
        null,
        {option: [{ type: 'STANDARD_COLOR', is_default: true }]},
        {option: [{ type: 'STANDARD_COLOR', is_default: true },
                  { type: 'CUSTOM_COLOR'}]},
        {option: [{ type: 'STANDARD_MONOCHROME', is_default: true },
                  { type: 'CUSTOM_MONOCHROME' }]},
        {option: [{ type: 'STANDARD_MONOCHROME', is_default: true}]},
      ].forEach(colorCap => {
        capabilities = getCdd();
        capabilities.printer.color = colorCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, colorElement.hidden);
      });

      // Custom color and monochrome options should make the section visible.
      capabilities = getCdd();
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
      let capabilities = getCdd();
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, mediaSizeElement.hidden);

      // Reset
      capabilities = getCdd();
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
      let capabilities = getCdd();
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, dpiElement.hidden);

      // Does not show up for only 1 option.
      capabilities = getCdd();
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
      let capabilities = getCdd();
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
      capabilities = getCdd();
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
      capabilities = getCdd();
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, duplex.hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
