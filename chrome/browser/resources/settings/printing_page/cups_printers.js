// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers' is a component for showing CUPS
 * Printer settings subpage (chrome://md-settings/cupsPrinters). It is used to
 * set up legacy & non-CloudPrint printers on ChromeOS by leveraging CUPS (the
 * unix printing system) and the many open source drivers built for CUPS.
 */
// TODO(xdai): Rename it to 'settings-cups-printers-page'.
Polymer({
  is: 'settings-cups-printers',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    searchTerm: {
      type: String,
    },
  },

  /** @override */
  ready: function() {
    this.updateCupsPrintersList_();
    this.addWebUIListener('on-add-cups-printer', this.onAddPrinter_.bind(this));
  },

  /**
   * @param {boolean} success
   * @param {string} printerName
   * @private
   */
  onAddPrinter_: function(success, printerName) {
    if (success) {
      this.updateCupsPrintersList_();
      var message = this.$.addPrinterDoneMessage;
      message.textContent = loadTimeData.getStringF(
          'printerAddedSuccessfulMessage', printerName);
    } else {
      var message = this.$.addPrinterErrorMessage;
    }
    message.hidden = false;
    window.setTimeout(function() {
      message.hidden = true;
    }, 3000);
  },

  /** @private */
  updateCupsPrintersList_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().
        getCupsPrintersList().then(this.printersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  printersChanged_: function(cupsPrintersList) {
    this.printers = cupsPrintersList.printerList;
  },

  /** @private */
  onAddPrinterTap_: function() {
    this.$.addPrinterDialog.open();
    this.$.addPrinterErrorMessage.hidden = true;
  },
});
