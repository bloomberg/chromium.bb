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
const AddPrinterDialogs = {
  DISCOVERY: 'add-printer-discovery-dialog',
  MANUALLY: 'add-printer-manually-dialog',
  CONFIGURING: 'add-printer-configuring-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
};

/**
 * The maximum height of the discovered printers list when the searching spinner
 * is not showing.
 * @type {number}
 */
const kPrinterListFullHeight = 350;

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
  return {
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerManufacturer: '',
    printerModel: '',
    printerMakeAndModel: '',
    printerName: '',
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerPpdReferenceResolved: false,
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
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .startDiscoveringPrinters();
    this.addWebUIListener(
        'on-printer-discovered', this.onPrinterDiscovered_.bind(this));
    this.addWebUIListener(
        'on-printer-discovery-done', this.onPrinterDiscoveryDone_.bind(this));
  },

  close: function() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {!Array<!CupsPrinterInfo>} printers
   * @private
   */
  onPrinterDiscovered_: function(printers) {
    this.discovering_ = true;
    this.discoveredPrinters = printers;
  },

  /** @private */
  onPrinterDiscoveryDone_: function() {
    this.discovering_ = false;
    this.$$('add-printer-list').style.maxHeight = kPrinterListFullHeight + 'px';
    this.$.noPrinterMessage.hidden = !!this.discoveredPrinters.length;

    if (!this.discoveredPrinters.length) {
      this.selectedPrinter = getEmptyPrinter_();
      this.fire('no-detected-printer');
    }
  },

  /** @private */
  stopDiscoveringPrinters_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .stopDiscoveringPrinters();
    this.discovering_ = false;
  },

  /** @private */
  switchToManualAddDialog_: function() {
    this.stopDiscoveringPrinters_();
    // We're abandoning discovery in favor of manual specification, so
    // drop the selection if one exists.
    this.selectedPrinter = getEmptyPrinter_();
    this.close();
    this.fire('open-manually-add-printer-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.stopDiscoveringPrinters_();
    this.close();
  },

  /** @private */
  switchToConfiguringDialog_: function() {
    this.stopDiscoveringPrinters_();
    this.close();
    this.fire('open-configuring-printer-dialog');
  },

  /**
   * @param {?CupsPrinterInfo} selectedPrinter
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_: function(selectedPrinter) {
    return !!selectedPrinter && !!selectedPrinter.printerName;
  },
});

Polymer({
  is: 'add-printer-manually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {type: Object, notify: true, value: getEmptyPrinter_},
  },

  /** @private */
  switchToDiscoveryDialog_: function() {
    this.newPrinter = getEmptyPrinter_();
    this.$$('add-printer-dialog').close();
    this.fire('open-discovery-printers-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  addPressed_: function() {
    // Set the default printer queue to be "ipp/print".
    if (!this.newPrinter.printerQueue) {
      this.set('newPrinter.printerQueue', 'ipp/print');
    }
    this.$$('add-printer-dialog').close();
    this.fire('open-configuring-printer-dialog');
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('newPrinter.printerProtocol', event.target.value);
  },

  /**
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_: function() {
    return settings.printing.isNameAndAddressValid(this.newPrinter);
  },
});

Polymer({
  is: 'add-printer-manufacturer-model-dialog',

  behaviors: [
    SetManufacturerModelBehavior,
  ],

  close: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onCancelTap_: function() {
    this.close();
    settings.CupsPrintersBrowserProxyImpl.getInstance().cancelPrinterSetUp(
        this.activePrinter);
  },

  /** @private */
  switchToConfiguringDialog_: function() {
    this.close();
    this.fire('open-configuring-printer-dialog');
  },

  /**
   * @param {string} ppdManufacturer
   * @param {string} ppdModel
   * @param {string} printerPPDPath
   * @return {boolean} Whether we have enough information to set up the printer
   * @private
   */
  canAddPrinter_: function(ppdManufacturer, ppdModel, printerPPDPath) {
    return settings.printing.isPPDInfoValid(
        ppdManufacturer, ppdModel, printerPPDPath);
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
    this.$.configuringMessage.textContent =
        loadTimeData.getStringF('printerConfiguringMessage', this.printerName);
  },

  /** @private */
  onCloseConfiguringTap_: function() {
    this.close();
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

    configuringDialogTitle: String,

    /** @private {string} */
    previousDialog_: String,

    /** @private {string} */
    currentDialog_: String,

    /** @private {boolean} */
    showDiscoveryDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManuallyAddDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showConfiguringDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManufacturerDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'open-manually-add-printer-dialog': 'openManuallyAddPrinterDialog_',
    'open-configuring-printer-dialog': 'openConfiguringPrinterDialog_',
    'open-discovery-printers-dialog': 'openDiscoveryPrintersDialog_',
    'open-manufacturer-model-dialog': 'openManufacturerModelDialog_',
    'no-detected-printer': 'onNoDetectedPrinter_',
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('on-add-cups-printer', this.onAddPrinter_.bind(this));
    this.addWebUIListener(
        'on-manually-add-discovered-printer',
        this.onManuallyAddDiscoveredPrinter_.bind(this));
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
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter_();
    }
  },

  /** @private */
  openManuallyAddPrinterDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  },

  /** @private */
  openDiscoveryPrintersDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.DISCOVERY,
        'showDiscoveryDialog_');
  },

  /** @private */
  addPrinter_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().addCupsPrinter(
        this.newPrinter);
  },

  /** @private */
  switchToManufacturerDialog_: function() {
    this.$$('add-printer-configuring-dialog').close();
    this.fire('open-manufacturer-model-dialog');
  },

  /**
   * Handler for getPrinterInfo success.
   * @param {!PrinterMakeModel} info
   * @private
   * */
  onPrinterFound_: function(info) {
    this.newPrinter.printerManufacturer = info.manufacturer;
    this.newPrinter.printerModel = info.model;
    this.newPrinter.printerMakeAndModel = info.makeAndModel;
    this.newPrinter.printerPpdReference.userSuppliedPpdUrl =
        info.ppdRefUserSuppliedPpdUrl;
    this.newPrinter.printerPpdReference.effectiveMakeAndModel =
        info.ppdRefEffectiveMakeAndModel;
    this.newPrinter.printerPpdReference.autoconf = info.autoconf;
    this.newPrinter.printerPpdReferenceResolved = info.ppdReferenceResolved;

    // Add the printer if it's configurable. Otherwise, forward to the
    // manufacturer dialog.
    if (this.newPrinter.printerPpdReferenceResolved) {
      this.addPrinter_();
    } else {
      this.switchToManufacturerDialog_();
    }
  },

  /**
   * Handler for getPrinterInfo failure.
   * @param {*} rejected
   * @private
   */
  infoFailed_: function(rejected) {
    this.switchToManufacturerDialog_();
  },

  /** @private */
  openConfiguringPrinterDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.CONFIGURING,
        'showConfiguringDialog_');
    if (this.previousDialog_ == AddPrinterDialogs.DISCOVERY) {
      this.configuringDialogTitle =
          loadTimeData.getString('addPrintersNearbyTitle');
      settings.CupsPrintersBrowserProxyImpl.getInstance().addDiscoveredPrinter(
          this.newPrinter.printerId);
    } else if (this.previousDialog_ == AddPrinterDialogs.MANUFACTURER) {
      this.configuringDialogTitle =
          loadTimeData.getString('selectManufacturerAndModelTitle');
      this.addPrinter_();
    } else if (this.previousDialog_ == AddPrinterDialogs.MANUALLY) {
      this.configuringDialogTitle =
          loadTimeData.getString('addPrintersManuallyTitle');
      if (this.newPrinter.printerProtocol == 'ipp' ||
          this.newPrinter.printerProtocol == 'ipps') {
        settings.CupsPrintersBrowserProxyImpl.getInstance()
            .getPrinterInfo(this.newPrinter)
            .then(this.onPrinterFound_.bind(this), this.infoFailed_.bind(this));
      } else {
        // Defer the switch until all the elements are drawn.
        this.async(this.switchToManufacturerDialog_.bind(this));
      }
    }
  },

  /** @private */
  openManufacturerModelDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  },

  /** @private */
  onNoDetectedPrinter_: function() {
    // If there is no detected printer, automatically open manually-add-printer
    // dialog only when the user opens the discovery-dialog through the
    // "ADD PRINTER" button.
    if (!this.previousDialog_) {
      this.$$('add-printer-discovery-dialog').close();
      this.newPrinter = getEmptyPrinter_();
      this.openManuallyAddPrinterDialog_();
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
      const dialog = this.$$(toDialog);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  },

  /**
   * Use the given printer as the starting point for a user-driven
   * add of a printer.  This is called if we can't automatically configure
   * the printer, and need more information from the user.
   *
   * @param {!CupsPrinterInfo} printer
   * @private
   */
  onManuallyAddDiscoveredPrinter_: function(printer) {
    this.newPrinter = printer;
    this.switchToManufacturerDialog_();
  },

  /**
   * @param {boolean} success
   * @param {string} printerName
   * @private
   */
  onAddPrinter_: function(success, printerName) {
    // 'on-add-cups-printer' event might be triggered by editing an existing
    // printer, in which case there is no configuring dialog.
    if (this.$$('add-printer-configuring-dialog')) {
      this.$$('add-printer-configuring-dialog').close();
    }
  },
});
