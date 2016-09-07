// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-discovery-dialog' is a dialog showing discovered printers on
 *   the network that are available for setup.
 * - 'add-printer-maually-dialog' is a dialog in which user can manually enter
 *   the information to set up a new printer.
 * - 'add-printer-configuring-dialog' is the configuring-in-progress dialog.
 * - 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 */

/**
 * Different dialogs in add printer flow.
 * @enum {string}
 */
var AddPrinterDialogs = {
  DISCOVERY: 'add-printer-discovery-dialog',
  MANUALLY: 'add-printer-maually-dialog',
  CONFIGURING: 'add-printer-configuring-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
};

Polymer({
  is: 'add-printer-discovery-dialog',

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    discoveredPrinters: {
      type: Array,
    },

    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
      notify: true,
    },
  },

  /** @override */
  ready: function() {
    // TODO(xdai): Get the discovered printer list after the API is ready.
  },

  /** @private */
  switchToManualAddDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-manually-add-printer-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  switchToConfiguringDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-configuring-printer-dialog');
  },
});

Polymer({
  is: 'add-printer-maually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
      notify: true,
      value: function() {
        return {
         printerAddress: '',
         printerDescription: '',
         printerId: '',
         printerManufacturer: '',
         printerModel: '',
         printerName: '',
         printerProtocol: 'ipp',
         printerQueue: '',
         printerStatus: '',
        };
      },
    },
  },

  /** @private */
  switchToDiscoveryDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-discovery-printers-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  switchToManufacturerDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-manufacturer-model-dialog');
  },

  /** @private */
  onAddressChanged_: function() {
    this.$.searchInProgress.hidden = false;
    this.$.searchFound.hidden = true;
    this.$.searchNotFound.hidden = true;

    var value = this.$.printerAddressInput.value;
    if (this.isValidIpAddress_(value)) {
      // TODO(xdai): Check if the printer address exists after the API is ready.
      this.$.searchInProgress.hidden = true;
      this.$.searchFound.hidden = false;
      this.$.searchNotFound.hidden = true;
    } else {
      this.$.searchInProgress.hidden = true;
      this.$.searchFound.hidden = true;
      this.$.searchNotFound.hidden = false;
    }
  },

  /**
   * @param {string} ip
   * @return {boolean}
   * @private
   */
  isValidIpAddress_: function(ip) {
    var addressRegex = RegExp('^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.' +
                              '([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.' +
                              '([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.' +
                              '([01]?\\d\\d?|2[0-4]\\d|25[0-5])$');
    return addressRegex.test(ip);
  },

  /**
   * @param {string} printerName
   * @param {string} printerAddress
   * @return {boolean}
   * @private
   */
  addPrinterNotAllowed_: function(printerName, printerAddress) {
    return !printerName || !printerAddress ||
           !this.isValidIpAddress_(printerAddress);
  },
});

Polymer({
  is: 'add-printer-manufacturer-model-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
      notify: true,
    },

    /** @type {!Array<string>} */
    manufacturerList: {
      type: Array,
    },

    /** @type {!Array<string>} */
    modelList: {
      type: Array,
    },
  },

  observers: [
    'selectedManufacturerChanged_(newPrinter.printerManufacturer)',
  ],

  /** @override */
  ready: function() {
    // TODO(xdai): Get available manufacturerList after the API is ready.
  },

  /** @private */
  selectedManufacturerChanged_: function() {
    // TODO(xdai): Get available modelList for a selected manufacturer after
    // the API is ready.
  },

  /** @private */
  switchToManualAddDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-manually-add-printer-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  switchToConfiguringDialog_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-configuring-printer-dialog');
  },
});

Polymer({
  is: 'add-printer-configuring-dialog',

  properties: {
    printerName: String,
  },

  /** @override */
  attached: function() {
    this.$.configuringMessage.textContent = loadTimeData.getStringF(
        'printerConfiguringMessage', this.printerName);
  },

  /** @private */
  onCancelConfiguringTap_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('configuring-dialog-closed');
  },
});

Polymer({
  is: 'settings-cups-add-printer-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
    },

    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
    },

    /** @private {string} */
    previousDialog_: String,

    /** @private {string} */
    currentDialog_: String,

    /** @private {boolean} */
    showDiscoveryDialog_: Boolean,

    /** @private {boolean} */
    showManuallyAddDialog_: Boolean,

    /** @private {boolean} */
    showConfiguringDialog_: Boolean,

    /** @private {boolean} */
    showManufacturerDialog_: Boolean,
  },

  listeners: {
    'configuring-dialog-closed': 'configuringDialogClosed_',
    'open-manually-add-printer-dialog': 'openManuallyAddPrinterDialog_',
    'open-configuring-printer-dialog': 'openConfiguringPrinterDialog_',
    'open-discovery-printers-dialog': 'openDiscoveryPrintersDialog_',
    'open-manufacturer-model-dialog': 'openManufacturerModelDialog_',
  },

  /** Opens the Add printer discovery dialog. */
  open: function() {
    this.switchDialog_('', AddPrinterDialogs.DISCOVERY, 'showDiscoveryDialog_');
  },

  /** @private */
  openManuallyAddPrinterDialog_: function() {
    this.switchDialog_(this.currentDialog_, AddPrinterDialogs.MANUALLY,
                       'showManuallyAddDialog_');
  },

  /** @private */
  openDiscoveryPrintersDialog_: function() {
    this.switchDialog_(this.currentDialog_, AddPrinterDialogs.DISCOVERY,
                       'showDiscoveryDialog_');
  },

  /** @private */
  openConfiguringPrinterDialog_: function() {
    this.switchDialog_(this.currentDialog_, AddPrinterDialogs.CONFIGURING,
                       'showConfiguringDialog_');
  },

  /** @private */
  openManufacturerModelDialog_: function() {
    this.switchDialog_(this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
                       'showManufacturerDialog_');
  },

  /** @private */
  configuringDialogClosed_: function() {
    if (this.previousDialog_ == AddPrinterDialogs.DISCOVERY) {
      this.switchDialog_(
          this.currentDialog_, this.previousDialog_, 'showDiscoveryDialog_');
    } else if (this.previousDialog_ == AddPrinterDialogs.MANUALLY) {
      this.switchDialog_(
          this.currentDialog_, this.previousDialog_, 'showManuallyAddDialog_');
    } else if (this.previousDialog_ == AddPrinterDialogs.MANUFACTURER) {
      this.switchDialog_(
          this.currentDialog_, this.previousDialog_, 'showManufacturerDialog_');
    }
  },

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param {string} fromDialog
   * @param {string} toDialog
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   * @private
   */
  switchDialog_: function(fromDialog, toDialog, domIfBooleanName) {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);
    this.async(function() {
      var dialog = this.$$(toDialog);
      dialog.addEventListener('close', function() {
        this.set(domIfBooleanName, false);
      }.bind(this));
    });
  },

  /**
   * @return {string} The name of the current printer in configuration.
   * @private
   */
  getConfiguringPrinterName_: function() {
    if (this.previousDialog_ == AddPrinterDialogs.DISCOVERY)
      return this.selectedPrinter.printerName;
    if (this.previousDialog_ == AddPrinterDialogs.MANUALLY ||
        this.previousDialog_ == AddPrinterDialogs.MANUFACTURER) {
      return this.newPrinter.printerName;
    }
    return '';
  },
});
