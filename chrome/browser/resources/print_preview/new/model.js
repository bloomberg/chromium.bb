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
 *    marginsType: (print_preview.ticket_items.MarginsTypeValue | undefined),
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

(function() {
'use strict';

/** @type {number} Number of recent destinations to save. */
const NUM_DESTINATIONS = 3;

/**
 * Sticky setting names. Alphabetical except for fitToPage, which must be set
 * after scaling in updateFromStickySettings().
 * @type {Array<string>}
 */
const STICKY_SETTING_NAMES = [
  'collate', 'color', 'cssBackground', 'dpi', 'duplex', 'headerFooter',
  'layout', 'margins', 'mediaSize', 'scaling', 'fitToPage'
];

Polymer({
  is: 'print-preview-model',

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {!print_preview_new.Settings}
     */
    settings: {
      type: Object,
      notify: true,
      value: {
        pages: {
          value: [1],
          unavailableValue: [],
          valid: true,
          available: true,
          key: '',
        },
        copies: {
          value: '1',
          unavailableValue: '1',
          valid: true,
          available: true,
          key: '',
        },
        collate: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isCollateEnabled',
        },
        layout: {
          value: false, /* portrait */
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isLandscapeEnabled',
        },
        color: {
          value: true, /* color */
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isColorEnabled',
        },
        mediaSize: {
          value: {
            width_microns: 215900,
            height_microns: 279400,
          },
          unavailableValue: {},
          valid: true,
          available: true,
          key: 'mediaSize',
        },
        margins: {
          value: print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          unavailableValue: print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          valid: true,
          available: true,
          key: 'marginsType',
        },
        dpi: {
          value: {},
          unavailableValue: {},
          valid: true,
          available: true,
          key: 'dpi',
        },
        fitToPage: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isFitToPageEnabled',
        },
        scaling: {
          value: '100',
          unavailableValue: '100',
          valid: true,
          available: true,
          key: 'scaling',
        },
        duplex: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isDuplexEnabled',
        },
        cssBackground: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isCssBackgroundEnabled',
        },
        selectionOnly: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: '',
        },
        headerFooter: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isHeaderFooterEnabled',
        },
        rasterize: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: '',
        },
        vendorItems: {
          value: {},
          unavailableValue: {},
          valid: true,
          available: true,
          key: '',
        },
        // This does not represent a real setting value, and is used only to
        // expose the availability of the other options settings section.
        otherOptions: {
          value: null,
          unavailableValue: null,
          valid: true,
          available: true,
        },
        // This does not represent a real settings value, but is used to
        // propagate the correctly formatted ranges for print tickets.
        ranges: {
          value: [],
          unavailableValue: [],
          valid: true,
          available: true,
          key: '',
        },
      },
    },

    /** @type {print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
    },

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinations: {
      type: Array,
      notify: true,
      value: [],
    },

    /** @type {print_preview.DocumentInfo} */
    documentInfo: {
      type: Object,
      notify: true,
    },
  },

  observers: [
    'updateSettings_(' +
        'destination.id, destination.capabilities, ' +
        'documentInfo.isModifiable, documentInfo.hasCssMediaStyles,' +
        'documentInfo.hasSelection)',
    'updateRecentDestinations_(destination, destination.capabilities)',
    'stickySettingsChanged_(' +
        'settings.collate.value, settings.layout.value, settings.color.value,' +
        'settings.mediaSize.value, settings.margins.value, ' +
        'settings.dpi.value, settings.fitToPage.value, ' +
        'settings.scaling.value, settings.duplex.value, ' +
        'settings.headerFooter.value, settings.cssBackground.value)',
  ],

  /** @private {boolean} */
  initialized_: false,

  /**
   * Updates the availability of the settings sections and values of dpi and
   *     media size settings.
   * @private
   */
  updateSettings_: function() {
    const caps = (!!this.destination && !!this.destination.capabilities) ?
        this.destination.capabilities.printer :
        null;
    this.updateSettingsAvailability_(caps);
    this.updateSettingsValues_(caps);
  },

  /**
   * @param {?print_preview.CddCapabilities} caps The printer capabilities.
   * @private
   */
  updateSettingsAvailability_: function(caps) {
    const isSaveToPdf = this.destination.id ==
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
    const knownSizeToSaveAsPdf = isSaveToPdf &&
        (!this.documentInfo.isModifiable ||
         this.documentInfo.hasCssMediaStyles);
    this.set('settings.copies.available', !!caps && !!(caps.copies));
    this.set('settings.collate.available', !!caps && !!(caps.collate));
    this.set('settings.layout.available', this.isLayoutAvailable_(caps));
    this.set('settings.color.available', this.destination.hasColorCapability);
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

  /**
   * @param {?print_preview.CddCapabilities} caps The printer capabilities.
   * @private
   */
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

  /**
   * @param {?print_preview.CddCapabilities} caps The printer capabilities.
   * @private
   */
  updateSettingsValues_: function(caps) {
    if (this.settings.mediaSize.available) {
      for (const option of caps.media_size.option) {
        if (option.is_default) {
          this.set('settings.mediaSize.value', option);
          break;
        }
      }
    }

    if (this.settings.dpi.available) {
      for (const option of caps.dpi.option) {
        if (option.is_default) {
          this.set('settings.dpi.value', option);
          break;
        }
      }
    } else if (
        caps && caps.dpi && caps.dpi.option && caps.dpi.option.length > 0) {
      this.set('settings.dpi.value', caps.dpi.option[0]);
    }
  },

  /** @private */
  updateRecentDestinations_: function() {
    if (!this.initialized_)
      return;

    // Determine if this destination is already in the recent destinations,
    // and where in the array it is located.
    const newDestination =
        print_preview.makeRecentDestination(assert(this.destination));
    let indexFound = this.recentDestinations.findIndex(function(recent) {
      return (
          newDestination.id == recent.id &&
          newDestination.origin == recent.origin);
    });

    // No change
    if (indexFound == 0 &&
        this.recentDestinations[0].capabilities ==
            newDestination.capabilities) {
      return;
    }

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (indexFound == -1 &&
        this.recentDestinations.length == NUM_DESTINATIONS) {
      indexFound = NUM_DESTINATIONS - 1;
    }
    if (indexFound != -1)
      this.recentDestinations.splice(indexFound, 1);

    // Add the most recent destination
    this.recentDestinations.splice(0, 0, newDestination);
    this.notifyPath('recentDestinations');

    // Persist sticky settings.
    this.stickySettingsChanged_();
  },

  /**
   * @param {?string} savedSettingsStr The sticky settings from native layer
   */
  updateFromStickySettings: function(savedSettingsStr) {
    this.initialized_ = true;
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
    if (savedSettings.version != 2)
      return;

    let recentDestinations = savedSettings.recentDestinations || [];
    if (!Array.isArray(recentDestinations)) {
      recentDestinations = [recentDestinations];
    }
    this.recentDestinations = recentDestinations;

    // Reset initialized, or stickySettingsChanged_ will get called for
    // every setting that gets set below.
    this.initialized_ = false;
    STICKY_SETTING_NAMES.forEach(settingName => {
      const setting = this.get(settingName, this.settings);
      const value = savedSettings[setting.key];
      if (value != undefined)
        this.set(`settings.${settingName}.value`, value);
    });
    this.initialized_ = true;
  },

  /** @private */
  stickySettingsChanged_: function() {
    if (!this.initialized_)
      return;

    const serialization = {
      version: 2,
      recentDestinations: this.recentDestinations,
    };

    STICKY_SETTING_NAMES.forEach(settingName => {
      const setting = this.get(settingName, this.settings);
      serialization[assert(setting.key)] = setting.value;
    });
    this.fire('save-sticky-settings', JSON.stringify(serialization));
  },
});
})();
