// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * @typedef {{
 *    version: string,
 *    recentDestinations: (!Array<!print_preview.RecentDestination> |
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
  is: 'print-preview-app',

  behaviors: [SettingsBehavior],
  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {!Object}
     */
    settings: {
      type: Object,
      notify: true,
    },

    /** @private {print_preview.DocumentInfo} */
    documentInfo_: {
      type: Object,
      notify: true,
    },

    /** @private {print_preview.Destination} */
    destination_: {
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

    /** @private {!print_preview_new.State} */
    state_: {
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

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {?print_preview.UserInfo} */
  userInfo_: null,

  /** @private {?WebUIListenerTracker} */
  listenerTracker_: null,

  /** @type {!print_preview.MeasurementSystem} */
  measurementSystem_: new print_preview.MeasurementSystem(
      ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL),

  /** @private {!Array<!print_preview.RecentDestination>} */
  recentDestinations_: [],

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance(),
    this.documentInfo_ = new print_preview.DocumentInfo();
    this.userInfo_ = new print_preview.UserInfo();
    this.listenerTracker_ = new WebUIListenerTracker();
    this.destinationStore_ = new print_preview.DestinationStore(
        this.userInfo_, this.listenerTracker_);
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  },

  /** @override */
  detached: function() {
    this.listenerTracker_.removeAll();
  },

  /**
   * @param {!print_preview.NativeInitialSettings} settings
   * @private
   */
  onInitialSettingsSet_: function(settings) {
    this.documentInfo_.init(
        settings.previewModifiable, settings.documentTitle,
        settings.documentHasSelection);
    // Temporary setting, will be replaced when page count is known from
    // the page-count-ready webUI event.
    this.documentInfo_.updatePageCount(5);
    this.notifyPath('documentInfo_.isModifiable');
    // Before triggering the final notification for settings availability,
    // set initialized = true.
    this.notifyPath('documentInfo_.hasSelection');
    this.notifyPath('documentInfo_.title');
    this.notifyPath('documentInfo_.pageCount');
    this.updateFromStickySettings_(settings.serializedAppStateStr);
    this.measurementSystem_.setSystem(
        settings.thousandsDelimeter, settings.decimalDelimeter,
        settings.unitType);
    this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
    this.destinationStore_.init(
        settings.isInAppKioskMode, settings.printerName,
        settings.serializedDefaultDestinationSelectionRulesStr,
        this.recentDestinations_);
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

    this.recentDestinations_ = savedSettings.recentDestinations || [];
    if (!Array.isArray(this.recentDestinations_))
      this.recentDestinations_ = [this.recentDestinations_];

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
});
