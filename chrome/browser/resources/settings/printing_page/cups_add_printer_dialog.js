// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-discovery-dialog' is a dialog showing discovered printers on
 *   the network that are available for setup.
 * - 'add-printer-manually-dialog' is a dialog in which user can manually enter
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
  MANUALLY: 'add-printer-manually-dialog',
  CONFIGURING: 'add-printer-configuring-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
};

/**
 * The maximum height of the discovered printers list when the searching spinner
 * is not showing.
 * @const {number}
 */
var kPrinterListFullHeight = 350;

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
  return {
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerManufacturer: '',
    printerModel: '',
    printerName: '',
    printerPPDPath: '',
    printerProtocol: 'ipp',
    printerQueue: '',
    printerStatus: '',
  };
}

Polymer({
  is: 'add-printer-discovery-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>|undefined} */
    discoveredPrinters: {
      type: Array,
    },

    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
      notify: true,
    },

    discovering_: {
      type: Boolean,
      value: true,
    },
  },

  /** @override */
  ready: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().
        startDiscoveringPrinters();
    this.addWebUIListener('on-printer-discovered',
                          this.onPrinterDiscovered_.bind(this));
    this.addWebUIListener('on-printer-discovery-done',
                          this.onPrinterDiscoveryDone_.bind(this));
    this.addWebUIListener('on-printer-discovery-failed',
                          this.onPrinterDiscoveryDone_.bind(this));
  },

  /**
   * @param {!Array<!CupsPrinterInfo>} printers
   * @private
   */
  onPrinterDiscovered_: function(printers) {
    this.discovering_ = true;
    if (!this.discoveredPrinters) {
      this.discoveredPrinters = printers;
    } else {
      for (var i = 0; i < printers.length; i++)
        this.push('discoveredPrinters', printers[i]);
    }
  },

  /** @private */
  onPrinterDiscoveryDone_: function() {
    this.discovering_ = false;
    this.$$('add-printer-list').style.maxHeight = kPrinterListFullHeight + 'px';
    this.$.noPrinterMessage.hidden = !!this.discoveredPrinters;
  },

  /** @private */
  stopDiscoveringPrinters_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().
        stopDiscoveringPrinters();
    this.discovering_ = false;
  },

  /** @private */
  switchToManualAddDialog_: function() {
    this.stopDiscoveringPrinters_();
    // We're abandoning discovery in favor of manual specification, so
    // drop the selection if one exists.
    this.selectedPrinter = getEmptyPrinter_();
    this.$$('add-printer-dialog').close();
    this.fire('open-manually-add-printer-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.stopDiscoveringPrinters_();
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  switchToManufacturerDialog_: function() {
    this.stopDiscoveringPrinters_();
    // If we're switching to the manufacturer/model dialog, clear the existing
    // data we have about the PPD (if any), as we're dropping that in favor of
    // user selections.
    this.selectedPrinter.printerManufacturer = '';
    this.selectedPrinter.printerModel = '';
    this.selectedPrinter.printerPPDPath = '';
    this.$$('add-printer-dialog').close();
    this.fire('open-manufacturer-model-dialog');
  },
});

Polymer({
  is: 'add-printer-manually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
      notify: true,
      value: getEmptyPrinter_
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
    // Set the default printer queue to be "ipp/print".
    if (!this.newPrinter.printerQueue)
      this.set('newPrinter.printerQueue', 'ipp/print');

    this.$$('add-printer-dialog').close();
    this.fire('open-manufacturer-model-dialog');
  },

  /** @private */
  onAddressChanged_: function() {
    // TODO(xdai): Check if the printer address exists and then show the
    // corresponding message after the API is ready.
    // The format of address is: ip-address-or-hostname:port-number.
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('newPrinter.printerProtocol', event.target.value);
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

    /** @type {?Array<string>} */
    manufacturerList: {
      type: Array,
    },

    /** @type {?Array<string>} */
    modelList: {
      type: Array,
    },

    setupFailed: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'selectedManufacturerChanged_(newPrinter.printerManufacturer)',
  ],

  /** @override */
  ready: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().
        getCupsPrinterManufacturersList().then(
            this.manufacturerListChanged_.bind(this));
  },

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedManufacturerChanged_: function(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('newPrinter.printerModel', '');
    if (manufacturer) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /** @private */
  onBrowseFile_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().
        getCupsPrinterPPDPath().then(this.printerPPDPathChanged_.bind(this));
  },

  /**
   * @param {string} path
   * @private
   */
  printerPPDPathChanged_: function(path) {
    this.set('newPrinter.printerPPDPath', path);
    this.$$('paper-input').value = this.getBaseName_(path);
  },

  /**
   * @param {!ManufacturersInfo} manufacturersInfo
   * @private
   */
  manufacturerListChanged_: function(manufacturersInfo) {
    if (manufacturersInfo.success)
      this.manufacturerList = manufacturersInfo.manufacturers;
  },

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_: function(modelsInfo) {
    if (modelsInfo.success)
      this.modelList = modelsInfo.models;
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

  /**
   * @param {string} path The full path of the file
   * @return {string} The base name of the file
   * @private
   */
  getBaseName_: function(path) {
    return path.substring(path.lastIndexOf('/') + 1);
  },

  /**
   * @param {string} printerManufacturer
   * @param {string} printerModel
   * @param {string} printerPPDPath
   * @return {boolean} Whether we have enough information to set up the printer
   * @private
   */
   canAddPrinter_: function(printerManufacturer, printerModel, printerPPDPath) {
     return !!((printerManufacturer && printerModel) || printerPPDPath);
   },
});

Polymer({
  is: 'add-printer-configuring-dialog',

  properties: {
    printerName: String,
    dialogTitle: String,
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

  close: function() {
    this.$$('add-printer-dialog').close();
  },
});

Polymer({
  is: 'settings-cups-add-printer-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
    },

    /** @type {boolean} whether the new printer setup is failed. */
    setupFailed: {
      type: Boolean,
      value: false,
    },

    configuringDialogTitle: String,

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

  /** @override */
  ready: function() {
    this.addWebUIListener('on-add-cups-printer', this.onAddPrinter_.bind(this));
  },

  /** Opens the Add printer discovery dialog. */
  open: function() {
    this.resetData_();
    this.switchDialog_('', AddPrinterDialogs.DISCOVERY, 'showDiscoveryDialog_');
  },

  /**
   * Reset all the printer data in the Add printer flow.
   * @private
   */
  resetData_: function() {
    if (this.newPrinter)
      this.newPrinter = getEmptyPrinter_();
    this.setupFailed = false;
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
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.CONFIGURING,
        'showConfiguringDialog_');
    if (this.previousDialog_ == AddPrinterDialogs.DISCOVERY) {
      this.configuringDialogTitle =
          loadTimeData.getString('addPrintersNearbyTitle');
      settings.CupsPrintersBrowserProxyImpl.getInstance().addCupsPrinter(
          this.newPrinter);
    } else if (this.previousDialog_ == AddPrinterDialogs.MANUFACTURER) {
      this.configuringDialogTitle =
          loadTimeData.getString('addPrintersManuallyTitle');
      settings.CupsPrintersBrowserProxyImpl.getInstance().addCupsPrinter(
          this.newPrinter);
    }
  },

  /** @private */
  openManufacturerModelDialog_: function() {
    this.switchDialog_(this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
                       'showManufacturerDialog_');
  },

  /** @private */
  configuringDialogClosed_: function() {
    // If the configuring dialog is closed, we want to return whence we came.
    //
    // TODO(justincarlson) - This shouldn't need to be a conditional;
    // clean up the way we switch dialogs so we don't have to supply
    // redundant information and can just return to the previous
    // dialog.
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
   * @param {boolean} success
   * @param {string} printerName
   * @private
   */
  onAddPrinter_: function(success, printerName) {
    this.$$('add-printer-configuring-dialog').close();
    if (success)
      return;

    if (this.previousDialog_ == AddPrinterDialogs.MANUFACTURER) {
      this.setupFailed = true;
    }
  },
});
