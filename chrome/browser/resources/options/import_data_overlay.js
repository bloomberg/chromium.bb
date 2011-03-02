// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this,
                     'importData',
                     templateData.importDataOverlayTabTitle,
                     'import-data-overlay');
  }

  cr.addSingletonGetter(ImportDataOverlay);

  ImportDataOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].onchange = function() {
          self.validateCommitButton_();
        };
      }

      $('import-browsers').onchange = function() {
        self.updateCheckboxes_();
        self.validateCommitButton_();
      };

      $('import-data-commit').onclick = function() {
        chrome.send('importData', [
            String($('import-browsers').selectedIndex),
            String($('import-history').checked),
            String($('import-favorites').checked),
            String($('import-passwords').checked),
            String($('import-search').checked)]);
      };

      $('import-data-cancel').onclick = function() {
        ImportDataOverlay.dismiss();
      };

      // Form controls are disabled until the profile list has been loaded.
      self.setControlsSensitive_(false);
    },

    /**
     * Set enabled and checked state of the commit button.
     * @private
     */
    validateCommitButton_: function() {
      var somethingToImport =
          $('import-history').checked || $('import-favorites').checked ||
          $('import-passwords').checked || $('import-search').checked;
      $('import-data-commit').disabled = !somethingToImport;
    },

    /**
     * Sets the sensitivity of all the checkboxes and the commit button.
     * @private
     */
    setControlsSensitive_: function(sensitive) {
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++)
        this.setUpCheckboxState_(checkboxes[i], sensitive);
      $('import-data-commit').disabled = !sensitive;
    },

    /**
     * Set enabled and checked states a checkbox element.
     * @param {Object} checkbox A checkbox element.
     * @param {boolean} enabled The enabled state of the chekbox.
     * @private
     */
    setUpCheckboxState_: function(checkbox, enabled) {
      checkbox.disabled = !enabled;
      checkbox.checked = enabled;
    },

    /**
     * Update the enabled and checked states of all checkboxes.
     * @private
     */
    updateCheckboxes_: function() {
      var index = $('import-browsers').selectedIndex;
      var browserProfile = ImportDataOverlay.browserProfiles[index];
      var importOptions = ['history', 'favorites', 'passwords', 'search'];
      for (var i = 0; i < importOptions.length; i++) {
        var checkbox = $('import-' + importOptions[i]);
        this.setUpCheckboxState_(checkbox, browserProfile[importOptions[i]]);
      }
    },

    /**
     * Update the supported browsers popup with given entries.
     * @param {array} browsers List of supported browsers name.
     * @private
     */
    updateSupportedBrowsers_: function(browsers) {
      ImportDataOverlay.browserProfiles = browsers;
      var browserSelect = $('import-browsers');
      browserSelect.remove(0);  // Remove the 'Loading...' option.
      browserSelect.textContent = '';
      var browserCount = browsers.length;

      if (browserCount == 0) {
        var option = new Option(templateData.noProfileFound, 0);
        browserSelect.appendChild(option);

        this.setControlsSensitive_(false);
      } else {
        this.setControlsSensitive_(true);
        for (var i = 0; i < browserCount; i++) {
          var browser = browsers[i]
          var option = new Option(browser['name'], browser['index']);
          browserSelect.appendChild(option);
        }

        this.updateCheckboxes_();
        this.validateCommitButton_();
      }
    },
  };

  /**
   * Update the supported browsers popup with given entries.
   * @param {array} list of supported browsers name.
   */
  ImportDataOverlay.updateSupportedBrowsers = function(browsers) {
    ImportDataOverlay.getInstance().updateSupportedBrowsers_(browsers);
  };

  /**
   * Update the UI to reflect whether an import operation is in progress.
   * @param {boolean} state True if an import operation is in progress.
   */
  ImportDataOverlay.setImportingState = function(state) {
    if (state) {
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].disabled = true;
      }
    } else {
      ImportDataOverlay.getInstance().updateCheckboxes_();
    }
    $('import-browsers').disabled = state;
    $('import-data-commit').disabled = state;
    $('import-throbber').style.visibility = state ? "visible" : "hidden";
  };

  /**
   * Remove the import overlay from display.
   */
  ImportDataOverlay.dismiss = function() {
    ImportDataOverlay.setImportingState(false);
    OptionsPage.closeOverlay();
  };

  // Export
  return {
    ImportDataOverlay: ImportDataOverlay
  };
});
