// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printer-details-page' is the subpage for
 * viewing the details of a CUPS printer.
 */
Polymer({
  is: 'settings-cups-printer-details-page',

  properties: {
    /** @type {!CupsPrinterInfo} */
    printer: {
      type: Object,
      notify: true,
    },
  },

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * Event triggered when the input value changes.
   * @private
   */
  onValueChanged_: function() {
    this.browserProxy_.updateCupsPrinter(this.printer.printerId,
                                         this.printer.printerName);
  },
});
