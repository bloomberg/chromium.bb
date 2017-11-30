// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Copies: 'copies',
    Layout: 'layout',
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

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertEquals(false, copiesElement.hidden);

      // Remove copies capability.
      const capabilities = getCdd();
      delete capabilities.printer.copies;

      // Copies section should now be hidden.
      page.set('destination.capabilities', capabilities);
      assertEquals(true, copiesElement.hidden);
    });

    test(assert(TestNames.Layout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');

      // Set up with HTML document.
      const htmlInfo = new print_preview.DocumentInfo();
      htmlInfo.init(true, 'title', false);
      page.set('documentInfo', htmlInfo);
      assertEquals(false, layoutElement.hidden);

      // Remove layout capability.
      let capabilities = getCdd();
      delete capabilities.printer.page_orientation;

      // Each of these settings should not show the capability.
      [
        null,
        {option: [{ type: 'PORTRAIT', is_default: true }]},
        {option: [{ type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        page.set('destination.capabilities', capabilities);
        assertEquals(true, layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities = getCdd();
      page.set('destination.capabilities', capabilities);
      assertEquals(false, layoutElement.hidden);

      // Test with PDF - should be hidden.
      const pdfInfo = new print_preview.DocumentInfo();
      pdfInfo.init(false, 'title', false);
      page.set('documentInfo', pdfInfo);
      assertEquals(true, layoutElement.hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
