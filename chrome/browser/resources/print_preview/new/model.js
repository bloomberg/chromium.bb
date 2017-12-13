// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * Must be kept in sync with the C++ MarginType enum in
 * printing/print_job_constants.h.
 * @enum {number}
 */
print_preview_new.MarginsTypeValue = {
  DEFAULT: 0,
  NO_MARGINS: 1,
  MINIMUM: 2,
  CUSTOM: 3
};

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
     *   otherOptions: !print_preview_new.Setting,
     * }}
     */
    settings: {
      type: Object,
      notify: true,
      value: {
        pages: {
          value: [1],
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
        // This does not represent a real setting value, and is used only to
        // expose the availability of the other options settings section.
        otherOptions: {
          value: null,
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
    },

    /** @type {print_preview.DocumentInfo} */
    documentInfo: {
      type: Object,
      notify: true,
    },
  },

  observers:
      ['updateSettingsAvailable_(' +
       'destination.id, destination.capabilities, ' +
       'documentInfo.isModifiable, documentInfo.hasCssMediaStyles,' +
       'documentInfo.hasSelection)'],
  /**
   * @private {!Array<string>} List of capability types considered color.
   * @const
   */
  COLOR_TYPES_: ['STANDARD_COLOR', 'CUSTOM_COLOR'],

  /**
   * @private {!Array<string>} List of capability types considered monochrome.
   * @const
   */
  MONOCHROME_TYPES_: ['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'],

  /**
   * Updates the availability of the settings sections.
   * @private
   */
  updateSettingsAvailable_: function() {
    const caps = (!!this.destination && !!this.destination.capabilities) ?
        this.destination.capabilities.printer :
        null;
    const isSaveToPdf = this.destination.id ==
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
    const knownSizeToSaveAsPdf = isSaveToPdf &&
        (!this.documentInfo.isModifiable ||
         this.documentInfo.hasCssMediaStyles);

    this.set('settings.copies.available', !!caps && !!(caps.copies));
    this.set('settings.collate.available', !!caps && !!(caps.collate));
    this.set('settings.layout.available', this.isLayoutAvailable_(caps));
    this.set('settings.color.available', this.isColorAvailable_(caps));
    this.set('settings.margins.available', this.documentInfo.isModifiable);
    this.set(
        'settings.mediaSize.available',
        !!caps && !!caps.media_size && !knownSizeToSaveAsPdf);
    this.set(
        'settings.dpi.available',
        !!caps && !!caps.dpi && !!caps.dpi.option &&
            caps.dpi.option.length > 1);
    this.set(
        'settings.fitToPage.available',
        !this.documentInfo.isModifiable && !isSaveToPdf);
    this.set('settings.scaling.available', !knownSizeToSaveAsPdf);
    this.set('settings.duplex.available', !!caps && !!caps.duplex);
    this.set(
        'settings.cssBackground.available', this.documentInfo.isModifiable);
    this.set(
        'settings.selectionOnly.available',
        this.documentInfo.isModifiable && this.documentInfo.hasSelection);
    this.set('settings.headerFooter.available', this.documentInfo.isModifiable);
    this.set(
        'settings.rasterize.available',
        !this.documentInfo.isModifiable && !cr.isWindows && !cr.isMac);
    this.set(
        'settings.otherOptions.available',
        this.settings.duplex.available ||
            this.settings.cssBackground.available ||
            this.settings.selectionOnly.available ||
            this.settings.headerFooter.available ||
            this.settings.rasterize.available);
  },

  /** @param {?print_preview.CddCapabilities} caps The printer capabilities. */
  isLayoutAvailable_: function(caps) {
    if (!caps || !caps.page_orientation || !caps.page_orientation.option ||
        !this.documentInfo.isModifiable ||
        this.documentInfo.hasCssMediaStyles) {
      return false;
    }
    let hasAutoOrPortraitOption = false;
    let hasLandscapeOption = false;
    caps.page_orientation.option.forEach(option => {
      hasAutoOrPortraitOption = hasAutoOrPortraitOption ||
          option.type == 'AUTO' || option.type == 'PORTRAIT';
      hasLandscapeOption = hasLandscapeOption || option.type == 'LANDSCAPE';
    });
    return hasLandscapeOption && hasAutoOrPortraitOption;
  },

  /** @param {?print_preview.CddCapabilities} caps The printer capabilities. */
  isColorAvailable_: function(caps) {
    if (!caps || !caps.color || !caps.color.option)
      return false;
    let hasColor = false;
    let hasMonochrome = false;
    caps.color.option.forEach(option => {
      const type = assert(option.type);
      hasColor = hasColor || this.COLOR_TYPES_.includes(option.type);
      hasMonochrome =
          hasMonochrome || this.MONOCHROME_TYPES_.includes(option.type);
    });
    return hasColor && hasMonochrome;
  },
});
