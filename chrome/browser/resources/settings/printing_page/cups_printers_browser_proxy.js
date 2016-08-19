// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser.
 */

/**
 * @typedef {{
 *   printerId: string,
 *   printerName: string,
 *   printerDescription: string,
 *   printerManufacturer: string,
 *   printerModel: string,
 *   printerStatus: string,
 *   printerAddress: string,
 *   printerProtocol: string,
 *   printerQueue: string
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
     */
    removeCupsPrinter: function(printerId) {},

  };

  /**
   * @constructor
   * @implements {settings.CupsPrintersBrowserProxy}
   */
  function CupsPrintersBrowserProxyImpl() {}
  cr.addSingletonGetter(CupsPrintersBrowserProxyImpl);

  CupsPrintersBrowserProxyImpl.prototype = {
    /** override */
    getCupsPrintersList: function() {
      return cr.sendWithPromise('getCupsPrintersList');
    },

    /** override */
    updateCupsPrinter: function(printerId, printerName) {
      chrome.send('updateCupsPrinter', [printerId, printerName]);
    },

    /** override */
    removeCupsPrinter: function(printerId) {
      chrome.send('removeCupsPrinter', [printerId]);
    },
  };

  return {
    CupsPrintersBrowserProxy: CupsPrintersBrowserProxy,
    CupsPrintersBrowserProxyImpl: CupsPrintersBrowserProxyImpl,
  };
});
