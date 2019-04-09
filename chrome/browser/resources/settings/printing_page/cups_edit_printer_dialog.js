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
    /**
     * The currently saved printer.
     * @type {CupsPrinterInfo}
     */
    activePrinter: Object,

    /**
     * Printer that holds the modified changes to activePrinter and only
     * applies these changes when the save button is clicked.
     * @type {CupsPrinterInfo}
     */
    pendingPrinter_: Object,

    /**
     * If the printer needs to be re-configured.
     * @private {boolean}
     */
    needsReconfigured_: {
      type: Boolean,
      value: false,
    },

    /**
     * The current PPD in use by the printer.
     * @private
     */
    userPPD_: String,


    /**
     * Tracks whether the dialog is fully initialized. This is required because
     * the dialog isn't fully initialized until Model and Manufacturer are set.
     * Allows us to ignore changes made to these fields until initialization is
     * complete.
     * @private
     */
    arePrinterFieldsInitialized_: {
      type: Boolean,
      value: false,
    },

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
      computed: 'isNetworkProtocol_(pendingPrinter_.printerProtocol)',
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
    'printerPathChanged_(pendingPrinter_.*)',
    'selectedEditManufacturerChanged_(pendingPrinter_.ppdManufacturer)',
    'onModelChanged_(pendingPrinter_.ppdModel)',
  ],

  /** @override */
  attached: function() {
    // Create a copy of activePrinter so that we can modify its fields.
    this.pendingPrinter_ = /** @type{CupsPrinterInfo} */
        (Object.assign({}, this.activePrinter));
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getPrinterPpdManufacturerAndModel(this.pendingPrinter_.printerId)
        .then(
            this.onGetPrinterPpdManufacturerAndModel_.bind(this),
            this.onGetPrinterPpdManufacturerAndModelFailed_.bind(this));
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
    this.userPPD_ =
        settings.printing.getBaseName(this.pendingPrinter_.printerPPDPath);
  },

  /**
   * @param {!{path: string, value: string}} change
   * @private
   */
  printerPathChanged_: function(change) {
    if (change.path != 'pendingPrinter_.printerName') {
      this.needsReconfigured_ = true;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('pendingPrinter_.printerProtocol', event.target.value);
    this.onPrinterInfoChange_();
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
    this.updateActivePrinter_();
    if (this.needsReconfigured_) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .reconfigureCupsPrinter(this.activePrinter);
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
    this.set('pendingPrinter_.ppdManufacturer', info.ppdManufacturer);
    this.set('pendingPrinter_.ppdModel', info.ppdModel);

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
    return this.printerInfoChanged_ && this.isPrinterValid();
  },

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedEditManufacturerChanged_: function(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('pendingPrinter_.ppdModel', '');
    this.modelList = [];
    if (!!manufacturer && manufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * Sets printerInfoChanged_ to true to show that the model has changed.
   * @private
   */
  onModelChanged_: function() {
    if (this.arePrinterFieldsInitialized_) {
      this.printerInfoChanged_ = true;
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
    if (this.pendingPrinter_.ppdManufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(this.pendingPrinter_.ppdManufacturer)
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
      // ModelListChanged_ is the final step of initializing pendingPrinter.
      this.arePrinterFieldsInitialized_ = true;
    }
  },

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_: function(path) {
    this.set('pendingPrinter_.printerPPDPath', path);
    this.invalidPPD_ = !path;
    if (!this.invalidPPD_) {
      // A new valid PPD file should be treated as a saveable change.
      this.onPrinterInfoChange_();
    }
    this.userPPD_ = settings.printing.getBaseName(path);
  },

  /*
   * Returns true if the printer has valid name, address, and PPD.
   * @return {boolean}
   */
  isPrinterValid: function() {
    return settings.printing.isNameAndAddressValid(this.pendingPrinter_) &&
        settings.printing.isPPDInfoValid(
            this.pendingPrinter_.ppdManufacturer, this.pendingPrinter_.ppdModel,
            this.pendingPrinter_.printerPPDPath);
  },

  /*
   * Helper function to copy over modified fields to activePrinter.
   * @private
   */
  updateActivePrinter_: function() {
    this.activePrinter =
        /** @type{CupsPrinterInfo} */ (Object.assign({}, this.pendingPrinter_));
    // Set ppdModel since there is an observer that clears ppdmodel's value when
    // ppdManufacturer changes.
    this.activePrinter.ppdModel = this.pendingPrinter_.ppdModel;
  },
});
