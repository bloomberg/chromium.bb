// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser.
 */

/**
 * @typedef {{
 *   ppdManufacturer: string,
 *   ppdModel: string,
 *   printerAddress: string,
 *   printerAutoconf: boolean,
 *   printerDescription: string,
 *   printerId: string,
 *   printerManufacturer: string,
 *   printerModel: string,
 *   printerMakeAndModel: string,
 *   printerName: string,
 *   printerPPDPath: string,
 *   printerProtocol: string,
 *   printerQueue: string,
 *   printerStatus: string,
 * }}
 */
var CupsPrinterInfo;

/**
 * @typedef {{
 *   printerList: !Array<!CupsPrinterInfo>,
 * }}
 */
var CupsPrintersList;

/**
 * @typedef {{
 *   success: boolean,
 *   manufacturers: Array<string>
 * }}
 */
var ManufacturersInfo;

/**
 * @typedef {{
 *   success: boolean,
 *   models: Array<string>
 * }}
 */
var ModelsInfo;

/**
 * @typedef {{
 *   manufacturer: string,
 *   model: string,
 *   makeAndModel: string,
 *   autoconf: boolean
 * }}
 */
var PrinterMakeModel;

/**
 * @typedef {{
 *   ppdManufacturer: string,
 *   ppdModel: string
 * }}
 */
var PrinterPpdMakeModel;

/**
 * @typedef {{
 *   message: string
 * }}
 */
var QueryFailure;

cr.define('settings', function() {
  /** @interface */
  class CupsPrintersBrowserProxy {
    /**
     * @return {!Promise<!CupsPrintersList>}
     */
    getCupsPrintersList() {}

    /**
     * @param {string} printerId
     * @param {string} printerName
     */
    updateCupsPrinter(printerId, printerName) {}

    /**
     * @param {string} printerId
     * @param {string} printerName
     */
    removeCupsPrinter(printerId, printerName) {}

    /**
     * @return {!Promise<string>} The full path of the printer PPD file.
     */
    getCupsPrinterPPDPath() {}

    /**
     * @param {!CupsPrinterInfo} newPrinter
     */
    addCupsPrinter(newPrinter) {}

    startDiscoveringPrinters() {}
    stopDiscoveringPrinters() {}

    /**
     * @return {!Promise<!ManufacturersInfo>}
     */
    getCupsPrinterManufacturersList() {}

    /**
     * @param {string} manufacturer
     * @return {!Promise<!ModelsInfo>}
     */
    getCupsPrinterModelsList(manufacturer) {}

    /**
     * @param {!CupsPrinterInfo} newPrinter
     * @return {!Promise<!PrinterMakeModel>}
     */
    getPrinterInfo(newPrinter) {}

    /**
     * @param {string} printerId
     * @return {!Promise<!PrinterPpdMakeModel>}
     */
    getPrinterPpdManufacturerAndModel(printerId) {}

    /**
     * @param{string} printerId
     */
    addDiscoveredPrinter(printerId) {}
  }

  /**
   * @implements {settings.CupsPrintersBrowserProxy}
   */
  class CupsPrintersBrowserProxyImpl {
    /** @override */
    getCupsPrintersList() {
      return cr.sendWithPromise('getCupsPrintersList');
    }

    /** @override */
    updateCupsPrinter(printerId, printerName) {
      chrome.send('updateCupsPrinter', [printerId, printerName]);
    }

    /** @override */
    removeCupsPrinter(printerId, printerName) {
      chrome.send('removeCupsPrinter', [printerId, printerName]);
    }

    /** @override */
    addCupsPrinter(newPrinter) {
      chrome.send('addCupsPrinter', [newPrinter]);
    }

    /** @override */
    getCupsPrinterPPDPath() {
      return cr.sendWithPromise('selectPPDFile');
    }

    /** @override */
    startDiscoveringPrinters() {
      chrome.send('startDiscoveringPrinters');
    }

    /** @override */
    stopDiscoveringPrinters() {
      chrome.send('stopDiscoveringPrinters');
    }

    /** @override */
    getCupsPrinterManufacturersList() {
      return cr.sendWithPromise('getCupsPrinterManufacturersList');
    }

    /** @override */
    getCupsPrinterModelsList(manufacturer) {
      return cr.sendWithPromise('getCupsPrinterModelsList', manufacturer);
    }

    /** @override */
    getPrinterInfo(newPrinter) {
      return cr.sendWithPromise('getPrinterInfo', newPrinter);
    }

    /** @override */
    getPrinterPpdManufacturerAndModel(printerId) {
      return cr.sendWithPromise('getPrinterPpdManufacturerAndModel', printerId);
    }

    /** @override */
    addDiscoveredPrinter(printerId) {
      chrome.send('addDiscoveredPrinter', [printerId]);
    }
  }

  cr.addSingletonGetter(CupsPrintersBrowserProxyImpl);

  return {
    CupsPrintersBrowserProxy: CupsPrintersBrowserProxy,
    CupsPrintersBrowserProxyImpl: CupsPrintersBrowserProxyImpl,
  };
});
