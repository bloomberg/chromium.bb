// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_test_utils', function() {

  /**
   * @param {string} printerId
   * @param {string=} opt_printerName Defaults to an empty string.
   * @return {!print_preview.PrinterCapabilitiesResponse}
   */
  function getCddTemplate(printerId, opt_printerName) {
    return {
      printer: {
        deviceName: printerId,
        printerName: opt_printerName || '',
      },
      capabilities: {
        version: '1.0',
        printer: {
          supported_content_type: [{content_type: 'application/pdf'}],
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
              {type: 'NO_DUPLEX', is_default: true},
              {type: 'LONG_EDGE'},
              {type: 'SHORT_EDGE'}
            ]
          },
          page_orientation: {
            option: [
              {type: 'PORTRAIT', is_default: true},
              {type: 'LANDSCAPE'},
              {type: 'AUTO'}
            ]
          },
          media_size: {
            option: [
              { name: 'NA_LETTER',
                width_microns: 215900,
                height_microns: 279400,
                is_default: true,
                custom_display_name: "Letter",
              },
              {
                name: 'CUSTOM_SQUARE',
                width_microns: 215900,
                height_microns: 215900,
                custom_display_name: "CUSTOM_SQUARE",
              }
            ]
          }
        }
      }
    };
  }

  return {
    getCddTemplate: getCddTemplate,
  };
});
