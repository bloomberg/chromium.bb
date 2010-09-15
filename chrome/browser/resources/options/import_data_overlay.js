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
      }

      $('import-data-commit').onclick = function() {
        chrome.send('importData', [
            String($('import-browsers').selectedIndex),
            String($('import-history').checked),
            String($('import-favorites').checked),
            String($('import-passwords').checked),
            String($('import-search').checked)]);
      }

      $('import-data-cancel').onclick = function() {
        ImportDataOverlay.dismiss();
      }
    },

    validateCommitButton_: function() {
      var somethingToImport =
          $('import-history').checked || $('import-favorites').checked ||
          $('import-passwords').checked || $('import-search').checked;
      $('import-data-commit').disabled = !somethingToImport;
    },

    updateCheckboxes_: function() {
      var index = $('import-browsers').selectedIndex;
      var browserProfile = ImportDataOverlay.browserProfiles[index];
      $('import-history').disabled = !browserProfile['history'];
      $('import-history').checked = browserProfile['history'];
      $('import-favorites').disabled = !browserProfile['favorites'];
      $('import-favorites').checked = browserProfile['favorites'];
      $('import-passwords').disabled = !browserProfile['passwords'];
      $('import-passwords').checked = browserProfile['passwords'];
      $('import-search').disabled = !browserProfile['search'];
      $('import-search').checked = browserProfile['search'];
    },

    /**
     * Update the supported browsers popup with given entries.
     * @param {Array} list of supported browsers name.
     */
    updateSupportedBrowsers_: function(browsers) {
      ImportDataOverlay.browserProfiles = browsers;
      browserSelect = $('import-browsers');
      browserSelect.textContent = '';
      browserCount = browsers.length;

      if (browserCount == 0) {
        var option = new Option(templateData.no_profile_found, 0);
        browserSelect.appendChild(option);

        var checkboxes =
            document.querySelectorAll(
            '#import-checkboxes input[type=checkbox]');
        for (var i = 0; i < checkboxes.length; i++) {
          checkboxes[i].checked = false;
          checkboxes[i].disabled = true;
        }
      } else {
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

  ImportDataOverlay.updateSupportedBrowsers = function(browsers) {
    ImportDataOverlay.getInstance().updateSupportedBrowsers_(browsers);
  };

  ImportDataOverlay.setImportingState = function(state) {
    if (state) {
      ImportDataOverlay.getInstance().updateCheckboxes_();
    } else {
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].disabled = true;
      }
    }
    $('import-browsers').disabled = state;
    $('import-data-commit').disabled = state;
    $('import-throbber').style.visibility = state ? "visible" : "hidden";

    function advanceThrobber() {
      var throbber = $('import-throbber');
      throbber.style.backgroundPositionX =
          ((parseInt(getComputedStyle(throbber).backgroundPositionX, 10) - 16)
          % 576) + 'px';
    }
    if (state) {
      ImportDataOverlay.throbIntervalId = setInterval(advanceThrobber, 30);
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
