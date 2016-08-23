// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-list' is a component for a list of
 * CUPS printers.
 */
Polymer({
  is: 'settings-cups-printers-list',

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

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onDetailsTap_: function(event) {
    this.closeDropdownMenu_();

    // Event is caught by 'settings-printing-page'.
    this.fire('show-cups-printer-details', event.model.item);
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onRemoveTap_: function(event) {
    this.closeDropdownMenu_();

    var index = this.printers.indexOf(event.model.item);
    this.splice('printers', index, 1);
    this.browserProxy_.removeCupsPrinter(event.model.item.printerId);
  },

  /** @private */
  closeDropdownMenu_: function() {
    this.$$('iron-dropdown').close();
  },

  /**
   * The filter callback function to show printers based on |searchTerm|.
   * @param {string} searchTerm
   * @private
   */
  filterPrinter_: function(searchTerm) {
    if (!searchTerm)
      return null;
    return function(printer) {
      return printer.printerName.toLowerCase().includes(
          searchTerm.toLowerCase());
    };
  },
});
