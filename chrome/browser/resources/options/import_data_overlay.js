// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * ImportDataOverlay class
   * Encapsulated handling of the 'Import Data' overlay page.
   * @class
   */
  function ImportDataOverlay() {
    OptionsPage.call(this, 'importDataOverlay',
                     templateData.import_data_title,
                     'importDataOverlay');
  }

  cr.addSingletonGetter(ImportDataOverlay);

  ImportDataOverlay.prototype = {
    // Inherit ImportDataOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('import-data-cancel').onclick = function(e) {
        OptionsPage.clearOverlays();
      }

      $('import-data-commit').onclick = function(e) {
        var paramList = new Array();
      }
    },

    /**
     * Clear the supported browsers popup
     * @private
     */
    clearSupportedBrowsers_: function() {
      $('supported-browsers').textContent = '';
    },

    /**
     * Update the supported browsers popup with given entries.
     * @param {Array} list of supported browsers name.
     */
    updateSupportedBrowsers_: function(browsers) {
      this.clearSupportedBrowsers_();
      browserSelect = $('supported-browsers');
      browserCount = browsers.length
      for (var i = 0; i < browserCount; i++) {
        var browser = browsers[i]
        var option = new Option(browser['name'], browser['index']);
        browserSelect.appendChild(option);
      }
    },
  };

  ImportDataOverlay.updateSupportedBrowsers = function(browsers) {
    ImportDataOverlay.getInstance().updateSupportedBrowsers_(browsers);
  }

  // Export
  return {
    ImportDataOverlay: ImportDataOverlay
  };

});

