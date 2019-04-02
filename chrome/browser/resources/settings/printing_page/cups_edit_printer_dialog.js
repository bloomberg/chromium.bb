// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-edit-printer-dialog' is a dialog to edit the
 * existing printer's information and re-configure it.
 */

Polymer({
  is: 'settings-cups-edit-printer-dialog',

  behaviors: [
    CrScrollableBehavior,
  ],

  properties: {
    /** @type {!CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    /**
     * If the printer needs to be re-configured.
     * @private {boolean}
     */
    needsReconfigured_: Boolean,

    /**
     * The current PPD in use by the printer.
     * @private
     */
    userPPD_: String,


    /**
     * If the printer info has changed since loading this dialog. This will
     * only track the freeform input fields, since the other fields contain
     * input selected from dropdown menus.
     * @private
     */
    printerInfoChanged_: {
      type: Boolean,
      value: false,
    },

    networkProtocolActive_: {
      type: Boolean,
      computed: 'isNetworkProtocol_(activePrinter.printerProtocol)',
    },

    /** @type {?Array<string>} */
    manufacturerList: Array,

    /** @type {?Array<string>} */
    modelList: Array,

    /**
     * Whether the user selected PPD file is valid.
     * @private
     */
    invalidPPD_: {
      type: Boolean,
      value: false,
    },

    /**
     * The base name of a newly selected PPD file.
     * @private
     */
    newUserPPD_: String,
  },

  observers: [
    'onPrinterInfoChange_(activePrinter.*)',
    'selectedEditManufacturerChanged_(activePrinter.ppdManufacturer)',
  ],

  /** @override */
  attached: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getPrinterPpdManufacturerAndModel(this.activePrinter.printerId)
        .then(
            this.onGetPrinterPpdManufacturerAndModel_.bind(this),
            this.onGetPrinterPpdManufacturerAndModelFailed_.bind(this));
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
    this.userPPD_ =
        settings.printing.getBaseName(this.activePrinter.printerPPDPath);
  },

  /**
   * @param {!{path: string, value: string}} change
   * @private
   */
  printerPathChanged_: function(change) {
    if (change.path != 'activePrinter.printerName') {
      this.needsReconfigured_ = true;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('activePrinter.printerProtocol', event.target.value);
  },

  /** @private */
  onPrinterInfoChange_: function() {
    this.printerInfoChanged_ = true;
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onSaveTap_: function() {
    if (this.needsReconfigured_) {
      settings.CupsPrintersBrowserProxyImpl.getInstance().addCupsPrinter(
          this.activePrinter);
    } else {
      settings.CupsPrintersBrowserProxyImpl.getInstance().updateCupsPrinter(
          this.activePrinter.printerId, this.activePrinter.printerName);
    }
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {!CupsPrinterInfo} printer
   * @return {string} The printer's URI that displays in the UI
   * @private
   */
  getPrinterURI_: function(printer) {
    if (!printer) {
      return '';
    } else if (
        printer.printerProtocol && printer.printerAddress &&
        printer.printerQueue) {
      return printer.printerProtocol + '://' + printer.printerAddress + '/' +
          printer.printerQueue;
    } else if (printer.printerProtocol && printer.printerAddress) {
      return printer.printerProtocol + '://' + printer.printerAddress;
    } else {
      return '';
    }
  },

  /**
   * Handler for getPrinterPpdManufacturerAndModel() success case.
   * @param {!PrinterPpdMakeModel} info
   * @private
   */
  onGetPrinterPpdManufacturerAndModel_: function(info) {
    this.set('activePrinter.ppdManufacturer', info.ppdManufacturer);
    this.set('activePrinter.ppdModel', info.ppdModel);

    // |needsReconfigured_| needs to reset to false after |ppdManufacturer| and
    // |ppdModel| are initialized to their correct values.
    this.needsReconfigured_ = false;
  },

  /**
   * Handler for getPrinterPpdManufacturerAndModel() failure case.
   * @private
   */
  onGetPrinterPpdManufacturerAndModelFailed_: function() {
    this.needsReconfigured_ = false;
  },

  /**
   * @param {string} protocol
   * @return {boolean} Whether |protocol| is a network protocol
   * @private
   */
  isNetworkProtocol_: function(protocol) {
    return settings.printing.isNetworkProtocol(protocol);
  },

  /**
   * @return {boolean} Whether the Save button is enabled.
   * @private
   */
  canSavePrinter_: function() {
    return !this.printerInfoChanged_ ||
        (settings.printing.isNameAndAddressValid(this.activePrinter) &&
         settings.printing.isPPDInfoValid(
             this.activePrinter.ppdManufacturer, this.activePrinter.ppdModel,
             this.activePrinter.printerPPDPath));
  },

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedEditManufacturerChanged_: function(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('activePrinter.ppdModel', '');
    this.modelList = [];
    if (!!manufacturer && manufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /** @private */
  onBrowseFile_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterPPDPath()
        .then(this.printerPPDPathChanged_.bind(this));
  },

  /**
   * @param {!ManufacturersInfo} manufacturersInfo
   * @private
   */
  manufacturerListChanged_: function(manufacturersInfo) {
    if (!manufacturersInfo.success) {
      return;
    }
    this.manufacturerList = manufacturersInfo.manufacturers;
    if (this.activePrinter.ppdManufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(this.activePrinter.ppdManufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_: function(modelsInfo) {
    if (modelsInfo.success) {
      this.modelList = modelsInfo.models;
    }
  },

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_: function(path) {
    this.set('activePrinter.printerPPDPath', path);
    this.invalidPPD_ = !path;
    this.userPPD_ = settings.printing.getBaseName(path);
  },
});
