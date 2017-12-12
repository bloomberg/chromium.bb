// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');
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

/**
 * @typedef {{id: string,
 *            origin: print_preview.DestinationOrigin,
 *            account: string,
 *            capabilities: ?print_preview.Cdd,
 *            displayName: string,
 *            extensionId: string,
 *            extensionName: string}}
 */
print_preview_new.RecentDestination;

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

/**
 * @typedef {{
 *    version: string,
 *    recentDestinations: (!Array<!print_preview_new.RecentDestination> |
 *                         undefined),
 *    dpi: ({horizontal_dpi: number,
 *           vertical_dpi: number,
 *           is_default: (boolean | undefined)} | undefined),
 *    mediaSize: ({height_microns: number,
 *                 width_microns: number,
 *                 custom_display_name: (string | undefined),
 *                 is_default: (boolean | undefined)} | undefined),
 *    marginsType: (print_preview_new.MarginsTypeValue | undefined),
 *    customMargins: ({marginTop: number,
 *                     marginBottom: number,
 *                     marginLeft: number,
 *                     marginRight: number} | undefined),
 *    isColorEnabled: (boolean | undefined),
 *    isDuplexEnabled: (boolean | undefined),
 *    isHeaderFooterEnabled: (boolean | undefined),
 *    isLandscapeEnabled: (boolean | undefined),
 *    isCollateEnabled: (boolean | undefined),
 *    isFitToPageEnabled: (boolean | undefined),
 *    isCssBackgroundEnabled: (boolean | undefined),
 *    scaling: (string | undefined),
 *    vendor_options: (Object | undefined)
 * }}
 */
print_preview_new.SerializedSettings;

Polymer({
  is: 'print-preview-model',

  behaviors: [SettingsBehavior],

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
      value: function() {
        const dest = new print_preview.Destination(
            'Foo Printer', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'Foo Printer', true,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'PrinterBrandAA 12345'});
        dest.capabilities = {
          version: '1.0',
          printer: {
            collate: {default: true},
            color: {
              option: [
                {type: 'STANDARD_COLOR', is_default: true},
                {type: 'STANDARD_MONOCHROME'}
              ]
            },
            copies: {default: 1, max: 1000},
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
          }
        };
        return dest;
      },
    },

    /** @type {print_preview.DocumentInfo} */
    documentInfo: {
      type: Object,
      notify: true,
      value: new print_preview.DocumentInfo(),
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

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @type {!print_preview.MeasurementSystem} */
  measurementSystem_: new print_preview.MeasurementSystem(
      ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL),

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance(),
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  },

  /**
   * @param {!print_preview.NativeInitialSettings} settings
   * @private
   */
  onInitialSettingsSet_: function(settings) {
    this.documentInfo.init(
        settings.previewModifiable, settings.documentTitle,
        settings.documentHasSelection);
    // Temporary setting, will be replaced when page count is known from
    // the page-count-ready webUI event.
    this.documentInfo.updatePageCount(5);
    this.notifyPath('documentInfo.isModifiable');
    // Before triggering the final notification for settings availability,
    // set initialized = true.
    this.notifyPath('documentInfo.hasSelection');
    this.notifyPath('documentInfo.title');
    this.notifyPath('documentInfo.pageCount');
    this.updateFromStickySettings_(settings.serializedAppStateStr);
    this.measurementSystem_.setSystem(
        settings.thousandsDelimeter, settings.decimalDelimeter,
        settings.unitType);
    this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
    // TODO(rbpotter): add destination store initialization.
  },

  /**
   * @param {?string} savedSettingsStr The sticky settings from native layer
   * @private
   */
  updateFromStickySettings_(savedSettingsStr) {
    if (!savedSettingsStr)
      return;
    let savedSettings;
    try {
      savedSettings = /** @type {print_preview_new.SerializedSettings} */ (
          JSON.parse(savedSettingsStr));
    } catch (e) {
      console.error('Unable to parse state ' + e);
      return;  // use default values rather than updating.
    }

    const updateIfDefined = (key1, key2) => {
      if (savedSettings[key2] != undefined)
        this.setSetting(key1, savedSettings[key2]);
    };
    [['dpi', 'dpi'], ['mediaSize', 'mediaSize'], ['margins', 'marginsType'],
     ['color', 'isColorEnabled'], ['headerFooter', 'isHeaderFooterEnabled'],
     ['layout', 'isLandscapeEnabled'], ['collate', 'isCollateEnabled'],
     ['fitToPage', 'isFitToPageEnabled'],
     ['cssBackground', 'isCssBackgroundEnabled'], ['scaling', 'scaling'],
    ].forEach(keys => updateIfDefined(keys[0], keys[1]));
  },

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
