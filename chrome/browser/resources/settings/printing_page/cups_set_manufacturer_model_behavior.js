// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview SetManufacturerModelBehavior for selecting manufacturer and
 * model from the available lists for a printer.
 */

/** @polymerBehavior */
const SetManufacturerModelBehavior = {
  properties: {
    /** @type {!CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    /** @type {?Array<string>} */
    manufacturerList: {
      type: Array,
    },

    /** @type {?Array<string>} */
    modelList: {
      type: Array,
    },
  },

  observers: [
    'selectedManufacturerChanged_(activePrinter.ppdManufacturer)',
  ],

  /** @override */
  attached: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
  },

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedManufacturerChanged_: function(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('activePrinter.ppdModel', '');
    this.modelList = [];
    if (manufacturer.length != 0) {
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
    if (!manufacturersInfo.success)
      return;
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
    if (modelsInfo.success)
      this.modelList = modelsInfo.models;
  },

  /**
   * @param {string} path
   * @private
   */
  printerPPDPathChanged_: function(path) {
    this.set('activePrinter.printerPPDPath', path);
  },

  /**
   * @param {string} path The full path of the file
   * @return {string} The base name of the file
   * @private
   */
  getBaseName_: function(path) {
    if (path && path.length > 0)
      return path.substring(path.lastIndexOf('/') + 1);
    else
      return '';
  },
};
