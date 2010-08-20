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

  ImportDataOverlay.throbIntervalId = 0
  ImportDataOverlay.checkboxMask = '';

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

      var self = this;
      var checkboxList =
          document.querySelectorAll('#checkboxList input[type=checkbox]');
      for (var i = 0; i < checkboxList.length; i++) {
        if(checkboxList[i].type == 'checkbox')
          checkboxList[i].onchange = function(e) {
            self.countCheckboxes_();
          };
      }

      $('import-data-commit').onclick = function(e) {
        /** The first digit in paramList indicates browser selected
         *  The rest indicate the checkboxes (1 is checked, 0 is not)
         */
        var selectedBrowser = $('supported-browsers').selectedIndex;
        var paramList =
            String(selectedBrowser) + ImportDataOverlay.checkboxMask;

        chrome.send('importData', [paramList]);
      }
    },

    countCheckboxes_: function() {
      ImportDataOverlay.checkboxMask = "";
      var checkboxList =
          document.querySelectorAll('#checkboxList input[type=checkbox]');
      for (var i = 0; i < checkboxList.length; i++) {
        if (checkboxList[i].type == 'checkbox') {
          if(checkboxList[i].checked)
            ImportDataOverlay.checkboxMask += "1";
          else
            ImportDataOverlay.checkboxMask += "0";
        }
      }
      if (ImportDataOverlay.checkboxMask.indexOf("1") == -1)
        $('import-data-commit').disabled = true;
      else
        $('import-data-commit').disabled = false;
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
      browserCount = browsers.length;

      if(browserCount == 0) {
        var option = new Option(templateData.no_profile_found, 0);
        browserSelect.appendChild(option);

        ImportDataOverlay.setDisable(true);
      }
      else {
        for (var i = 0; i < browserCount; i++) {
          var browser = browsers[i]
          var option = new Option(browser['name'], browser['index']);
          browserSelect.appendChild(option);

        ImportDataOverlay.setDisable(false);
        this.countCheckboxes_();
        }
      }
    },
  };

  ImportDataOverlay.loadImporter = function() {
    chrome.send('loadImporter');
  };

  ImportDataOverlay.updateSupportedBrowsers = function(browsers) {
    ImportDataOverlay.getInstance().updateSupportedBrowsers_(browsers);
  };

  ImportDataOverlay.setDisable = function(state) {
    $('supported-browsers').disabled = state;
    $('import-favorites').disabled = state;
    $('import-search').disabled = state;
    $('import-passwords').disabled = state;
    $('import-history').disabled = state;
    $('import-data-commit').disabled = state;
  };

  ImportDataOverlay.setImportingState = function(state) {
    ImportDataOverlay.setDisable(state);
    $('import-throbber').style.visibility = state ? "visible" : "hidden";

    function advanceThrobber() {
      var throbber = $('import-throbber');
      throbber.style.backgroundPositionX =
          ((parseInt(getComputedStyle(throbber).backgroundPositionX, 10) - 16)
          % 576) + 'px';
    }
    if (state) {
      ImportDataOverlay.throbIntervalId =
          setInterval(advanceThrobber, 30);
    } else {
      clearInterval(ImportDataOverlay.throbIntervalId);
    }
  };

  ImportDataOverlay.dismiss = function() {
    ImportDataOverlay.setImportingState(false);
    OptionsPage.clearOverlays();
  }

  // Export
  return {
    ImportDataOverlay: ImportDataOverlay
  };

});
