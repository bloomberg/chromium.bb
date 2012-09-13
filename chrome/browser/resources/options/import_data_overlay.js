// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
                     loadTimeData.getString('importDataOverlayTabTitle'),
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

      $('import-data-show-bookmarks-bar').onchange = function() {
        // Note: The callback 'toggleShowBookmarksBar' is handled within the
        // browser options handler -- rather than the import data handler --
        // as the implementation is shared by several clients.
        chrome.send('toggleShowBookmarksBar');
      }

      $('import-data-confirm').onclick = function() {
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
       checkbox.setDisabled('noProfileData', !enabled);
    },

    /**
     * Update the enabled and checked states of all checkboxes.
     * @private
     */
    updateCheckboxes_: function() {
      var index = $('import-browsers').selectedIndex;
      var browserProfile;
      if (this.browserProfiles.length > index)
        browserProfile = this.browserProfiles[index];
      var importOptions = ['history', 'favorites', 'passwords', 'search'];
      for (var i = 0; i < importOptions.length; i++) {
        var checkbox = $('import-' + importOptions[i]);
        var enable = browserProfile && browserProfile[importOptions[i]];
        this.setUpCheckboxState_(checkbox, enable);
      }
    },

    /**
     * Update the supported browsers popup with given entries.
     * @param {array} browsers List of supported browsers name.
     * @private
     */
    updateSupportedBrowsers_: function(browsers) {
      this.browserProfiles = browsers;
      var browserSelect = $('import-browsers');
      browserSelect.remove(0);  // Remove the 'Loading...' option.
      browserSelect.textContent = '';
      var browserCount = browsers.length;

      if (browserCount == 0) {
        var option = new Option(loadTimeData.getString('noProfileFound'), 0);
        browserSelect.appendChild(option);

        this.setControlsSensitive_(false);
      } else {
        this.setControlsSensitive_(true);
        for (var i = 0; i < browserCount; i++) {
          var browser = browsers[i];
          var option = new Option(browser.name, browser.index);
          browserSelect.appendChild(option);
        }

        this.updateCheckboxes_();
        this.validateCommitButton_();
      }
    },

    /**
     * Clear import prefs set when user checks/unchecks a checkbox so that each
     * checkbox goes back to the default "checked" state (or alternatively, to
     * the state set by a recommended policy).
     * @private
     */
    clearUserPrefs_: function() {
      var importPrefs = ['import_history',
                         'import_bookmarks',
                         'import_saved_passwords',
                         'import_search_engine'];
      for (var i = 0; i < importPrefs.length; i++)
        Preferences.clearPref(importPrefs[i], true);
    },

    /**
     * Update the dialog layout to reflect success state.
     * @param {boolean} success If true, show success dialog elements.
     * @private
     */
    updateSuccessState_: function(success) {
      var sections = document.querySelectorAll('.import-data-configure');
      for (var i = 0; i < sections.length; i++)
        sections[i].hidden = success;

      sections = document.querySelectorAll('.import-data-success');
      for (var i = 0; i < sections.length; i++)
        sections[i].hidden = !success;
    },
  };

  ImportDataOverlay.clearUserPrefs = function() {
    ImportDataOverlay.getInstance().clearUserPrefs_();
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
    var checkboxes =
        document.querySelectorAll('#import-checkboxes input[type=checkbox]');
    for (var i = 0; i < checkboxes.length; i++)
        checkboxes[i].setDisabled('Importing', state);
    if (!state)
      ImportDataOverlay.getInstance().updateCheckboxes_();
    $('import-browsers').disabled = state;
    $('import-throbber').style.visibility = state ? 'visible' : 'hidden';
    ImportDataOverlay.getInstance().validateCommitButton_();
  };

  /**
   * Remove the import overlay from display.
   */
  ImportDataOverlay.dismiss = function() {
    ImportDataOverlay.clearUserPrefs();
    OptionsPage.closeOverlay();
  };

  /**
   * Show a message confirming the success of the import operation.
   */
  ImportDataOverlay.confirmSuccess = function() {
    var showBookmarksMessage = $('import-favorites').checked;
    ImportDataOverlay.setImportingState(false);
    $('import-find-your-bookmarks').hidden = !showBookmarksMessage;
    ImportDataOverlay.getInstance().updateSuccessState_(true);
  };

  /**
   * Show the import data overlay.
   */
  ImportDataOverlay.show = function() {
    // Make sure that any previous import success message is hidden, and
    // we're showing the UI to import further data.
    ImportDataOverlay.getInstance().updateSuccessState_(false);

    OptionsPage.navigateToPage('importData');
  };

  // Export
  return {
    ImportDataOverlay: ImportDataOverlay
  };
});
