// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * @typedef {{
 *   value: *,
 *   valid: boolean,
 *   available: boolean,
 *   updatesPreview: boolean
 * }}
 */
print_preview_new.Setting;

/**
 * @typedef {{
 *   previewLoading: boolean,
 *   previewFailed: boolean,
 *   cloudPrintError: string,
 *   privetExtensionError: string,
 *   invalidSettings: boolean,
 * }}
 */
print_preview_new.State;

Polymer({
  is: 'print-preview-model',

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {{
     *   pages: !print_preview_new.Setting,
     *   copies: !print_preview_new.Setting,
     *   collate: !print_preview_new.Setting,
     *   layout: !print_preview_new.Setting,
     *   color: !print_preview_new.Setting,
     *   mediaSize: !print_preview_new.Setting,
     *   margins: !print_preview_new.Setting,
     *   dpi: !print_preview_new.Setting,
     *   fitToPage: !print_preview_new.Setting,
     *   scaling: !print_preview_new.Setting,
     *   duplex: !print_preview_new.Setting,
     *   cssBackground: !print_preview_new.Setting,
     *   selectionOnly: !print_preview_new.Setting,
     *   headerFooter: !print_preview_new.Setting,
     *   rasterize: !print_preview_new.Setting,
     *   vendorItems: !print_preview_new.Setting,
     * }}
     */
    settings: {
      type: Object,
      notify: true,
      value: {
        pages: {
          value: [1, 2, 3, 4, 5],
          valid: true,
          available: true,
          updatesPreview: true,
        },
        copies: {
          value: '1',
          valid: true,
          available: true,
          updatesPreview: false,
        },
        collate: {
          value: true,
          valid: true,
          available: true,
          updatesPreview: false,
        },
        layout: {
          value: false, /* portrait */
          valid: true,
          available: true,
          updatesPreview: true,
        },
        color: {
          value: true, /* color */
          valid: true,
          available: true,
          updatesPreview: true,
        },
        mediaSize: {
          value: {
            width_microns: 215900,
            height_microns: 279400,
          },
          valid: true,
          available: true,
          updatesPreview: true,
        },
        margins: {
          value: 0,
          valid: true,
          available: true,
          updatesPreview: true,
        },
        dpi: {
          value: {},
          valid: true,
          available: true,
          updatesPreview: false,
        },
        fitToPage: {
          value: false,
          valid: true,
          available: true,
          updatesPreview: true,
        },
        scaling: {
          value: '100',
          valid: true,
          available: true,
          updatesPreview: true,
        },
        duplex: {
          value: true,
          valid: true,
          available: true,
          updatesPreview: false,
        },
        cssBackground: {
          value: false,
          valid: true,
          available: true,
          updatesPreview: true,
        },
        selectionOnly: {
          value: false,
          valid: true,
          available: true,
          updatesPreview: true,
        },
        headerFooter: {
          value: true,
          valid: true,
          available: true,
          updatesPreview: true,
        },
        rasterize: {
          value: false,
          valid: true,
          available: true,
          updatesPreview: false,
        },
        vendorItems: {
          value: {},
          valid: true,
          available: true,
          updatesPreview: false,
        },
      },
    },

    /** @type {print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
      value: new print_preview.Destination(
          'Foo Printer', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'Foo Printer', true,
          print_preview.DestinationConnectionStatus.ONLINE,
          {description: 'PrinterBrandAA 12345'}),
    },

    /** @type {print_preview.DocumentInfo} */
    documentInfo: {
      type: Object,
      notify: true,
      value: function() {
        const info = new print_preview.DocumentInfo();
        info.init(false, 'DocumentTitle', true);
        info.updatePageCount(5);
        info.fitToPageScaling_ = 94;
        return info;
      },
    },

    /** @type {!print_preview_new.State} */
    state: {
      type: Object,
      notify: true,
      value: {
        previewLoading: false,
        previewFailed: false,
        cloudPrintError: '',
        privetExtensionError: '',
        invalidSettings: false,
      },
    },
  },
});
