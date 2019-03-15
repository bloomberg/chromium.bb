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
 *    customMargins: (print_preview.MarginsSetting | undefined),
 *    isColorEnabled: (boolean | undefined),
 *    isDuplexEnabled: (boolean | undefined),
 *    isHeaderFooterEnabled: (boolean | undefined),
 *    isLandscapeEnabled: (boolean | undefined),
 *    isCollateEnabled: (boolean | undefined),
 *    isFitToPageEnabled: (boolean | undefined),
 *    isCssBackgroundEnabled: (boolean | undefined),
 *    scaling: (string | undefined),
 *    vendor_options: (Object | undefined),
 *    isPinEnabled: (boolean | undefined)
 * }}
 */
print_preview_new.SerializedSettings;

/**
 * @typedef {{
 *  value: *,
 *  managed: boolean
 * }}
 */
print_preview_new.PolicyEntry;

/**
 * @typedef {{
 *   headerFooter: print_preview_new.PolicyEntry
 * }}
 */
print_preview_new.PolicySettings;

/**
 * Constant values matching printing::DuplexMode enum.
 * @enum {number}
 */
print_preview_new.DuplexMode = {
  SIMPLEX: 0,
  LONG_EDGE: 1,
  UNKNOWN_DUPLEX_MODE: -1
};

/**
 * Values matching the types of duplex in a CDD.
 * @enum {string}
 */
print_preview_new.DuplexType = {
  NO_DUPLEX: 'NO_DUPLEX',
  LONG_EDGE: 'LONG_EDGE',
  SHORT_EDGE: 'SHORT_EDGE'
};

(function() {
'use strict';

/**
 * Sticky setting names. Alphabetical except for fitToPage, which must be set
 * after scaling in updateFromStickySettings().
 * @type {!Array<string>}
 */
const STICKY_SETTING_NAMES = [
  'recentDestinations',
  'collate',
  'color',
  'cssBackground',
  'customMargins',
  'dpi',
  'duplex',
  'headerFooter',
  'layout',
  'margins',
  'mediaSize',
  'customScaling',
  'scaling',
  'fitToPage',
  'vendorItems',
];
// <if expr="chromeos">
STICKY_SETTING_NAMES.push('pin');
// </if>

/**
 * Minimum height of page in microns to allow headers and footers. Should
 * match the value for min_size_printer_units in printing/print_settings.cc
 * so that we do not request header/footer for margins that will be zero.
 * @type {number}
 */
const MINIMUM_HEIGHT_MICRONS = 25400;

Polymer({
  is: 'print-preview-model',

  behaviors: [SettingsBehavior],

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * Initialize settings that are only available on some printers to
     * unavailable, and settings that are provided by PDF generation to
     * available.
     * @type {!print_preview_new.Settings}
     */
    settings: {
      type: Object,
      notify: true,
      value: function() {
        const value = {
          pages: {
            value: [1],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          copies: {
            value: '1',
            unavailableValue: '1',
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          collate: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'isCollateEnabled',
          },
          layout: {
            value: false, /* portrait */
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'isLandscapeEnabled',
          },
          color: {
            value: true, /* color */
            unavailableValue: false,
            valid: true,
            available: false,
            setByPolicy: false,
            key: 'isColorEnabled',
          },
          mediaSize: {
            value: {},
            unavailableValue: {
              width_microns: 215900,
              height_microns: 279400,
            },
            valid: true,
            available: false,
            setByPolicy: false,
            key: 'mediaSize',
          },
          margins: {
            value: print_preview.ticket_items.MarginsTypeValue.DEFAULT,
            unavailableValue:
                print_preview.ticket_items.MarginsTypeValue.DEFAULT,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'marginsType',
          },
          customMargins: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'customMargins',
          },
          dpi: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: false,
            setByPolicy: false,
            key: 'dpi',
          },
          fitToPage: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'isFitToPageEnabled',
          },
          scaling: {
            value: '100',
            unavailableValue: '100',
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'scaling',
          },
          customScaling: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'customScaling',
          },
          duplex: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: false,
            setByPolicy: false,
            key: 'isDuplexEnabled',
          },
          cssBackground: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'isCssBackgroundEnabled',
          },
          selectionOnly: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          headerFooter: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'isHeaderFooterEnabled',
          },
          rasterize: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          vendorItems: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: false,
            setByPolicy: false,
            key: 'vendorOptions',
          },
          pagesPerSheet: {
            value: 1,
            unavailableValue: 1,
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          // This does not represent a real setting value, and is used only to
          // expose the availability of the other options settings section.
          otherOptions: {
            value: null,
            unavailableValue: null,
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          // This does not represent a real settings value, but is used to
          // propagate the correctly formatted ranges for print tickets.
          ranges: {
            value: [],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            key: '',
          },
          recentDestinations: {
            value: [],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            key: 'recentDestinations',
          },
        };
        // <if expr="chromeos">
        value.pin = {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isPinEnabled',
        };
        // </if>
        return value;
      },
    },

    controlsManaged: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** @type {print_preview.Destination} */
    destination: Object,

    /** @type {!print_preview.DocumentSettings} */
    documentSettings: Object,

    /** @type {print_preview.Margins} */
    margins: Object,

    /** @type {!print_preview.Size} */
    pageSize: Object,
  },

  observers: [
    'updateSettingsFromDestination_(destination.capabilities)',
    'updateSettingsAvailabilityFromDocumentSettings_(' +
        'documentSettings.isModifiable, documentSettings.hasCssMediaStyles,' +
        'documentSettings.hasSelection)',
    'updateHeaderFooterAvailable_(' +
        'margins, settings.margins.value, ' +
        'settings.customMargins.value, settings.mediaSize.value)',
    'stickySettingsChanged_(' +
        'settings.collate.value, settings.layout.value, settings.color.value,' +
        'settings.mediaSize.value, settings.margins.value, ' +
        'settings.customMargins.value, settings.dpi.value, ' +
        'settings.fitToPage.value, settings.customScaling.value, ' +
        'settings.scaling.value, settings.duplex.value, ' +
        'settings.headerFooter.value, settings.cssBackground.value, ' +
        'settings.vendorItems.value, settings.recentDestinations.value.*)',
    'stickySettingsChanged_(settings.pin.value)',
  ],

  /** @private {boolean} */
  initialized_: false,

  /** @private {?print_preview_new.SerializedSettings} */
  stickySettings_: null,

  /** @private {?print_preview_new.PolicySettings} */
  policySettings_: null,

  /** @private {?print_preview.Cdd} */
  lastDestinationCapabilities_: null,

  /**
   * Updates the availability of the settings sections and values of dpi and
   *     media size settings based on the destination capabilities.
   * @private
   */
  updateSettingsFromDestination_: function() {
    if (!this.destination) {
      return;
    }

    if (this.destination.capabilities == this.lastDestinationCapabilities_) {
      return;
    }

    this.lastDestinationCapabilities_ = this.destination.capabilities;

    const caps = this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    this.updateSettingsAvailabilityFromDestination_(caps);

    if (!caps) {
      return;
    }

    this.updateSettingsValues_(caps);
  },

  /**
   * @param {?print_preview.CddCapabilities} caps The printer capabilities.
   * @private
   */
  updateSettingsAvailabilityFromDestination_: function(caps) {
    this.set('settings.copies.available', !!caps && !!caps.copies);
    this.set('settings.collate.available', !!caps && !!caps.collate);
    this.set('settings.color.available', this.destination.hasColorCapability);

    this.set(
        'settings.dpi.available',
        !!caps && !!caps.dpi && !!caps.dpi.option &&
            caps.dpi.option.length > 1);

    this.set(
        'settings.duplex.available',
        !!caps && !!caps.duplex && !!caps.duplex.option &&
            caps.duplex.option.some(
                o => o.type == print_preview_new.DuplexType.LONG_EDGE) &&
            caps.duplex.option.some(
                o => o.type == print_preview_new.DuplexType.NO_DUPLEX));

    this.set(
        'settings.vendorItems.available', !!caps && !!caps.vendor_capability);

    // <if expr="chromeos">
    this.set(
        'settings.pin.available', !!caps && !!caps.pin && !!caps.pin.supported);
    // </if>

    if (this.documentSettings) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  },

  /** @private */
  updateSettingsAvailabilityFromDestinationAndDocumentSettings_: function() {
    const isSaveAsPDF = this.destination.id ==
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
    const knownSizeToSaveAsPdf = isSaveAsPDF &&
        (!this.documentSettings.isModifiable ||
         this.documentSettings.hasCssMediaStyles);
    this.set('settings.fitToPage.unavailableValue', !isSaveAsPDF);
    this.set(
        'settings.fitToPage.available',
        !knownSizeToSaveAsPdf && !this.documentSettings.isModifiable);
    this.set('settings.scaling.available', !knownSizeToSaveAsPdf);
    const caps = this.destination && this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    this.set(
        'settings.mediaSize.available',
        !!caps && !!caps.media_size && !knownSizeToSaveAsPdf);
    this.set('settings.layout.available', this.isLayoutAvailable_(caps));
    this.set(
        'settings.otherOptions.available',
        this.settings.duplex.available ||
            this.settings.cssBackground.available ||
            this.settings.selectionOnly.available ||
            this.settings.headerFooter.available ||
            this.settings.rasterize.available);
  },

  /** @private */
  updateSettingsAvailabilityFromDocumentSettings_: function() {
    this.set('settings.margins.available', this.documentSettings.isModifiable);
    this.set(
        'settings.customMargins.available', this.documentSettings.isModifiable);
    this.set(
        'settings.cssBackground.available', this.documentSettings.isModifiable);
    this.set(
        'settings.selectionOnly.available',
        this.documentSettings.isModifiable &&
            this.documentSettings.hasSelection);
    this.set(
        'settings.headerFooter.available', this.isHeaderFooterAvailable_());
    this.set(
        'settings.rasterize.available',
        !this.documentSettings.isModifiable && !cr.isWindows && !cr.isMac);

    if (this.destination) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  },

  /** @private */
  updateHeaderFooterAvailable_: function() {
    if (this.documentSettings === undefined) {
      return;
    }

    this.set(
        'settings.headerFooter.available', this.isHeaderFooterAvailable_());
  },

  /**
   * @return {boolean} Whether the header/footer setting should be available.
   * @private
   */
  isHeaderFooterAvailable_: function() {
    // Always unavailable for PDFs.
    if (!this.documentSettings.isModifiable) {
      return false;
    }

    // Always unavailable for small paper sizes.
    const microns = this.getSettingValue('layout') ?
        this.getSettingValue('mediaSize').width_microns :
        this.getSettingValue('mediaSize').height_microns;
    if (microns < MINIMUM_HEIGHT_MICRONS) {
      return false;
    }

    // Otherwise, availability depends on the margins.
    let available = false;
    const marginsType =
        /** @type {!print_preview.ticket_items.MarginsTypeValue} */ (
            this.getSettingValue('margins'));
    switch (marginsType) {
      case print_preview.ticket_items.MarginsTypeValue.DEFAULT:
        available = !this.margins ||
            this.margins.get(
                print_preview.ticket_items.CustomMarginsOrientation.TOP) > 0 ||
            this.margins.get(
                print_preview.ticket_items.CustomMarginsOrientation.BOTTOM) > 0;
        break;
      case print_preview.ticket_items.MarginsTypeValue.NO_MARGINS:
        break;
      case print_preview.ticket_items.MarginsTypeValue.MINIMUM:
        available = true;
        break;
      case print_preview.ticket_items.MarginsTypeValue.CUSTOM:
        const margins = this.getSettingValue('customMargins');
        available = margins.marginTop > 0 || margins.marginBottom > 0;
        break;
      default:
        break;
    }
    return available;
  },

  /**
   * @param {?print_preview.CddCapabilities} caps The printer capabilities.
   * @private
   */
  isLayoutAvailable_: function(caps) {
    if (!caps || !caps.page_orientation || !caps.page_orientation.option ||
        !this.documentSettings.isModifiable ||
        this.documentSettings.hasCssMediaStyles) {
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
      const defaultOption = caps.media_size.option.find(o => !!o.is_default) ||
          caps.media_size.option[0];
      let matchingOption = null;
      // If the setting does not have a valid value, the UI has just started so
      // do not try to get a matching value; just set the printer default in
      // case the user doesn't have sticky settings.
      if (this.settings.mediaSize.value.height_microns !== undefined) {
        const currentMediaSize = this.getSettingValue('mediaSize');
        matchingOption = caps.media_size.option.find(o => {
          return o.height_microns === currentMediaSize.height_microns &&
              o.width_microns === currentMediaSize.width_microns;
        });
      }
      this.setSetting('mediaSize', matchingOption || defaultOption);
    }

    if (this.settings.dpi.available) {
      const defaultOption =
          caps.dpi.option.find(o => !!o.is_default) || caps.dpi.option[0];
      let matchingOption = null;
      if (this.settings.dpi.value.horizontal_dpi !== undefined) {
        const currentDpi = this.getSettingValue('dpi');
        matchingOption = caps.dpi.option.find(o => {
          return o.horizontal_dpi === currentDpi.horizontal_dpi &&
              o.vertical_dpi === currentDpi.vertical_dpi;
        });
      }
      this.setSetting('dpi', matchingOption || defaultOption);
    } else if (
        caps && caps.dpi && caps.dpi.option && caps.dpi.option.length > 0) {
      this.set('settings.dpi.unavailableValue', caps.dpi.option[0]);
    }

    if (!this.initialized_ && this.settings.color.available) {
      const defaultOption = this.destination.defaultColorOption;
      if (defaultOption) {
        this.setSetting(
            'color',
            !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
                defaultOption.type));
      }
    } else if (
        !this.settings.color.available &&
        (this.destination.id ===
             print_preview.Destination.GooglePromotedId.DOCS ||
         this.destination.type === print_preview.DestinationType.MOBILE)) {
      this.set('settings.color.unavailableValue', true);
    } else if (
        !this.settings.color.available && caps && caps.color &&
        caps.color.option && caps.color.option.length > 0) {
      this.set(
          'settings.color.unavailableValue',
          !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
              caps.color.option[0].type));
    } else if (!this.settings.color.available) {
      // if no color capability is reported, assume black and white.
      this.set('settings.color.unavailableValue', false);
    }

    if (!this.initialized_ && this.settings.duplex.available) {
      const defaultOption = caps.duplex.option.find(o => !!o.is_default);
      this.setSetting(
          'duplex',
          defaultOption ?
              defaultOption.type == print_preview_new.DuplexType.LONG_EDGE :
              false);
    } else if (
        !this.settings.duplex.available && caps && caps.duplex &&
        caps.duplex.option &&
        !caps.duplex.option.some(
            o => o.type != print_preview_new.DuplexType.LONG_EDGE)) {
      // If the only option available is long edge, the value should always be
      // true.
      this.set('settings.duplex.unavailableValue', true);
    } else if (!this.settings.duplex.available) {
      // If no duplex capability is reported, assume false.
      this.set('settings.duplex.unavailableValue', false);
    }

    if (this.settings.vendorItems.available) {
      const vendorSettings = {};
      for (const item of caps.vendor_capability) {
        let defaultValue = null;
        if (item.type == 'SELECT' && item.select_cap &&
            item.select_cap.option) {
          const defaultOption =
              item.select_cap.option.find(o => !!o.is_default);
          defaultValue = defaultOption ? defaultOption.value : null;
        } else if (item.type == 'RANGE') {
          if (item.range_cap) {
            defaultValue = item.range_cap.default || null;
          }
        } else if (item.type == 'TYPED_VALUE') {
          if (item.typed_value_cap) {
            defaultValue = item.typed_value_cap.default || null;
          }
        }
        if (defaultValue != null) {
          vendorSettings[item.id] = defaultValue;
        }
      }
      this.setSetting('vendorItems', vendorSettings);
    }
  },

  /**
   * Caches the sticky settings and sets up the recent destinations. Sticky
   * settings will be applied when destinaton capabilities have been retrieved.
   * @param {?string} savedSettingsStr The sticky settings from native layer
   */
  setStickySettings: function(savedSettingsStr) {
    assert(!this.stickySettings_);

    if (!savedSettingsStr) {
      return;
    }

    let savedSettings;
    try {
      savedSettings = /** @type {print_preview_new.SerializedSettings} */ (
          JSON.parse(savedSettingsStr));
    } catch (e) {
      console.error('Unable to parse state ' + e);
      return;  // use default values rather than updating.
    }
    if (savedSettings.version != 2) {
      return;
    }

    let recentDestinations = savedSettings.recentDestinations || [];
    if (!Array.isArray(recentDestinations)) {
      recentDestinations = [recentDestinations];
    }
    // Initialize recent destinations early so that the destination store can
    // start trying to fetch them.
    this.setSetting('recentDestinations', recentDestinations);

    this.stickySettings_ = savedSettings;
  },

  /**
   * Sets settings in accordance to policies from native code, and prevents
   * those settings from being changed via other means.
   * @param {boolean|undefined} headerFooter Value of
   *     printing.print_header_footer, if set in prefs (or undefined, if not).
   * @param {boolean} isHeaderFooterManaged true if the header/footer UI state
   *     is managed by a policy.
   */
  setPolicySettings: function(headerFooter, isHeaderFooterManaged) {
    this.policySettings_ = {
      headerFooter: {
        value: headerFooter,
        managed: isHeaderFooterManaged,
      },
    };
  },

  applyStickySettings: function() {
    if (this.stickySettings_) {
      STICKY_SETTING_NAMES.forEach(settingName => {
        const setting = this.get(settingName, this.settings);
        const value = this.stickySettings_[setting.key];
        if (value != undefined) {
          this.setSetting(settingName, value);
        } else if (settingName === 'customScaling') {
          // Use the stored scaling value instead of resetting users with an
          // older set of sticky settings.
          this.setSetting(
              settingName, this.stickySettings_['scaling'] !== '100');
        }
      });
    }
    if (this.policySettings_) {
      for (const [settingName, policy] of Object.entries(
               this.policySettings_)) {
        if (policy.value !== undefined) {
          this.setSetting(settingName, policy.value);
        }
        if (policy.managed) {
          this.set(`settings.${settingName}.setByPolicy`, true);
        }
      }
    }
    this.initialized_ = true;
    this.updateManaged_();
    this.stickySettings_ = null;
    this.stickySettingsChanged_();
  },

  // <if expr="chromeos">
  /**
   * Restricts settings and applies defaults as defined by policy applicable to
   * current destination.
   */
  applyDestinationSpecificPolicies: function() {
    const colorPolicy = this.destination.colorPolicy;
    const colorValue =
        colorPolicy ? colorPolicy : this.destination.defaultColorPolicy;
    if (colorValue) {
      // |this.setSetting| does nothing if policy is present.
      // We want to set the value nevertheless so we call |this.set| directly.
      this.set(
          'settings.color.value', colorValue == print_preview.ColorMode.COLOR);
    }
    this.set('settings.color.setByPolicy', !!colorPolicy);

    const duplexPolicy = this.destination.duplexPolicy;
    const duplexValue =
        duplexPolicy ? duplexPolicy : this.destination.defaultDuplexPolicy;
    if (duplexValue) {
      this.set(
          'settings.duplex.value',
          duplexValue != print_preview.DuplexModeRestriction.SIMPLEX);
    }
    this.set('settings.duplex.setByPolicy', !!duplexPolicy);

    const pinPolicy = this.destination.pinPolicy;
    const pinValue = pinPolicy ? pinPolicy : this.destination.defaultPinPolicy;
    if (pinValue) {
      this.set(
          'settings.pin.value',
          pinValue == print_preview.PinModeRestriction.SECURE);
    }
    this.set('settings.pin.setByPolicy', !!pinPolicy);
    this.updateManaged_();
  },
  // </if>

  /** @private */
  updateManaged_: function() {
    let managedSettings = ['headerFooter'];
    // <if expr="chromeos">
    managedSettings = managedSettings.concat(['color', 'duplex', 'pin']);
    // </if>
    this.controlsManaged = managedSettings.some(settingName => {
      const setting = this.getSetting(settingName);
      return setting.available && setting.setByPolicy;
    });
  },

  /** @return {boolean} Whether the model has been initialized. */
  initialized: function() {
    return this.initialized_;
  },

  /** @private */
  stickySettingsChanged_: function() {
    if (!this.initialized_) {
      return;
    }

    const serialization = {
      version: 2,
    };

    STICKY_SETTING_NAMES.forEach(settingName => {
      const setting = this.get(settingName, this.settings);
      serialization[assert(setting.key)] = setting.value;
    });
    this.fire('save-sticky-settings', JSON.stringify(serialization));
  },

  /**
   * Creates a string that represents a print ticket.
   * @param {!print_preview.Destination} destination Destination to print to.
   * @param {boolean} openPdfInPreview Whether this print request is to open
   *     the PDF in Preview app (Mac only).
   * @param {boolean} showSystemDialog Whether this print request is to show
   *     the system dialog.
   * @return {string} Serialized print ticket.
   */
  createPrintTicket: function(destination, openPdfInPreview, showSystemDialog) {
    const dpi = /** @type {{horizontal_dpi: (number | undefined),
                            vertical_dpi: (number | undefined),
                            vendor_id: (number | undefined)}} */ (
        this.getSettingValue('dpi'));

    const ticket = {
      mediaSize: this.getSettingValue('mediaSize'),
      pageCount: this.getSettingValue('pages').length,
      landscape: this.getSettingValue('layout'),
      color: destination.getNativeColorModel(
          /** @type {boolean} */ (this.getSettingValue('color'))),
      headerFooterEnabled: false,  // only used in print preview
      marginsType: this.getSettingValue('margins'),
      duplex: this.getSettingValue('duplex') ?
          print_preview_new.DuplexMode.LONG_EDGE :
          print_preview_new.DuplexMode.SIMPLEX,
      copies: parseInt(this.getSettingValue('copies'), 10),
      collate: this.getSettingValue('collate'),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: false,  // only used in print preview
      previewModifiable: this.documentSettings.isModifiable,
      printToPDF: destination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
      printToGoogleDrive:
          destination.id == print_preview.Destination.GooglePromotedId.DOCS,
      printWithCloudPrint: !destination.isLocal,
      printWithPrivet: destination.isPrivet,
      printWithExtension: destination.isExtension,
      rasterizePDF: this.getSettingValue('rasterize'),
      scaleFactor: this.getSettingValue('customScaling') ?
          parseInt(this.getSettingValue('scaling'), 10) :
          100,
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      dpiHorizontal: (dpi && 'horizontal_dpi' in dpi) ? dpi.horizontal_dpi : 0,
      dpiVertical: (dpi && 'vertical_dpi' in dpi) ? dpi.vertical_dpi : 0,
      dpiDefault: (dpi && 'is_default' in dpi) ? dpi.is_default : false,
      deviceName: destination.id,
      fitToPageEnabled: this.getSettingValue('fitToPage'),
      pageWidth: this.pageSize.width,
      pageHeight: this.pageSize.height,
      showSystemDialog: showSystemDialog,
    };

    // Set 'cloudPrintID' only if the destination is not local.
    if (!destination.isLocal) {
      ticket.cloudPrintID = destination.id;
    }

    if (this.getSettingValue('margins') ==
        print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      ticket.marginsCustom = this.getSettingValue('customMargins');
    }

    if (destination.isPrivet || destination.isExtension) {
      // TODO(rbpotter): Get local and PDF printers to use the same ticket and
      // send only this ticket instead of nesting it in a larger ticket.
      ticket.ticket = this.createCloudJobTicket(destination);
      ticket.capabilities = JSON.stringify(destination.capabilities);
    }

    if (openPdfInPreview) {
      ticket.OpenPDFInPreview = true;
    }

    return JSON.stringify(ticket);
  },

  /**
   * Creates an object that represents a Google Cloud Print print ticket.
   * @param {!print_preview.Destination} destination Destination to print to.
   * @return {string} Google Cloud Print print ticket.
   */
  createCloudJobTicket: function(destination) {
    assert(
        !destination.isLocal || destination.isPrivet || destination.isExtension,
        'Trying to create a Google Cloud Print print ticket for a local ' +
            ' non-privet and non-extension destination');
    assert(
        destination.capabilities,
        'Trying to create a Google Cloud Print print ticket for a ' +
            'destination with no print capabilities');

    // Create CJT (Cloud Job Ticket)
    const cjt = {version: '1.0', print: {}};
    if (this.settings.collate.available) {
      cjt.print.collate = {collate: this.settings.collate.value};
    }
    if (this.settings.color.available) {
      const selectedOption = destination.getSelectedColorOption(
          /** @type {boolean} */ (this.settings.color.value));
      if (!selectedOption) {
        console.error('Could not find correct color option');
      } else {
        cjt.print.color = {type: selectedOption.type};
        if (selectedOption.hasOwnProperty('vendor_id')) {
          cjt.print.color.vendor_id = selectedOption.vendor_id;
        }
      }
    } else {
      // Always try setting the color in the print ticket, otherwise a
      // reasonable reader of the ticket will have to do more work, or process
      // the ticket sub-optimally, in order to safely handle the lack of a
      // color ticket item.
      const defaultOption = destination.defaultColorOption;
      if (defaultOption) {
        cjt.print.color = {type: defaultOption.type};
        if (defaultOption.hasOwnProperty('vendor_id')) {
          cjt.print.color.vendor_id = defaultOption.vendor_id;
        }
      }
    }
    if (this.settings.copies.available) {
      cjt.print.copies = {copies: parseInt(this.getSettingValue('copies'), 10)};
    }
    if (this.settings.duplex.available) {
      cjt.print.duplex = {
        type: this.settings.duplex.value ?
            print_preview_new.DuplexType.LONG_EDGE :
            print_preview_new.DuplexType.NO_DUPLEX,
      };
    }
    if (this.settings.mediaSize.available) {
      const mediaValue = this.settings.mediaSize.value;
      cjt.print.media_size = {
        width_microns: mediaValue.width_microns,
        height_microns: mediaValue.height_microns,
        is_continuous_feed: mediaValue.is_continuous_feed,
        vendor_id: mediaValue.vendor_id
      };
    }
    if (!this.settings.layout.available) {
      // In this case "orientation" option is hidden from user, so user can't
      // adjust it for page content, see Landscape.isCapabilityAvailable().
      // We can improve results if we set AUTO here.
      const capability = destination.capabilities.printer ?
          destination.capabilities.printer.page_orientation :
          null;
      if (capability && capability.option &&
          capability.option.some(option => option.type == 'AUTO')) {
        cjt.print.page_orientation = {type: 'AUTO'};
      }
    } else {
      cjt.print.page_orientation = {
        type: this.settings.layout.value ? 'LANDSCAPE' : 'PORTRAIT'
      };
    }
    if (this.settings.dpi.available) {
      const dpiValue = this.settings.dpi.value;
      cjt.print.dpi = {
        horizontal_dpi: dpiValue.horizontal_dpi,
        vertical_dpi: dpiValue.vertical_dpi,
        vendor_id: dpiValue.vendor_id
      };
    }
    if (this.settings.vendorItems.available) {
      const items = this.settings.vendorItems.value;
      cjt.print.vendor_ticket_item = [];
      for (const itemId in items) {
        if (items.hasOwnProperty(itemId)) {
          cjt.print.vendor_ticket_item.push({id: itemId, value: items[itemId]});
        }
      }
    }
    return JSON.stringify(cjt);
  },
});
})();
