// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser.
 */

/**
 * @typedef {{
 *   printerAddress: string,
 *   printerDescription: string,
 *   printerId: string,
 *   printerManufacturer: string,
 *   printerModel: string,
 *   printerName: string,
 *   printerPPDPath: string,
 *   printerProtocol: string,
 *   printerQueue: string,
 *   printerStatus: string
 * }}
 */
var CupsPrinterInfo;

/**
 * @typedef {{
 *   printerList: !Array<!CupsPrinterInfo>,
 * }}
 */
var CupsPrintersList;

cr.define('settings', function() {
  /** @interface */
  function CupsPrintersBrowserProxy() {}

  CupsPrintersBrowserProxy.prototype = {

    /**
     * @return {!Promise<!CupsPrintersList>}
     */
    getCupsPrintersList: function() {},

    /**
     * @param {string} printerId
     * @param {string} printerName
     */
    updateCupsPrinter: function(printerId, printerName) {},

    /**
     * @param {string} printerId
     * @param {string} printerName
     */
    removeCupsPrinter: function(printerId, printerName) {},

    /**
     * @return {!Promise<string>} The full path of the printer PPD file.
     */
    getCupsPrinterPPDPath: function() {},

    /**
     * @param {!CupsPrinterInfo} newPrinter
     */
    addCupsPrinter: function(newPrinter) {},

    startDiscoveringPrinters: function() {},

    stopDiscoveringPrinters: function() {},
  };

  /**
   * @constructor
   * @implements {settings.CupsPrintersBrowserProxy}
   */
  function CupsPrintersBrowserProxyImpl() {}
  cr.addSingletonGetter(CupsPrintersBrowserProxyImpl);

  CupsPrintersBrowserProxyImpl.prototype = {
    /** @override */
    getCupsPrintersList: function() {
      return cr.sendWithPromise('getCupsPrintersList');
    },

    /** @override */
    updateCupsPrinter: function(printerId, printerName) {
      chrome.send('updateCupsPrinter', [printerId, printerName]);
    },

    /** @override */
    removeCupsPrinter: function(printerId, printerName) {
      chrome.send('removeCupsPrinter', [printerId, printerName]);
    },

    /** @override */
    addCupsPrinter: function(newPrinter) {
      chrome.send('addCupsPrinter', [newPrinter]);
    },

    /** @override */
    getCupsPrinterPPDPath: function() {
      return cr.sendWithPromise('selectPPDFile');
    },

    /** @override */
    startDiscoveringPrinters: function() {
      chrome.send('startDiscoveringPrinters');
    },

    /** @override */
    stopDiscoveringPrinters: function() {
      chrome.send('stopDiscoveringPrinters');
    },
  };

  return {
    CupsPrintersBrowserProxy: CupsPrintersBrowserProxy,
    CupsPrintersBrowserProxyImpl: CupsPrintersBrowserProxyImpl,
  };
});
