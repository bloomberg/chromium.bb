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
    SetManufacturerModelBehavior,
  ],

  /** @override */
  ready: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getPrinterPpdManufacturerAndModel(this.activePrinter.printerId)
        .then(this.onGetPrinterPpdManufacturerAndModel_.bind(this));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('activePrinter.printerProtocol', event.target.value);
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onSaveTap_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().addCupsPrinter(
        this.activePrinter);
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
  },
});
