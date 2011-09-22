// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for observing function of the backend datasource for this page by
// tests.
var webui_responded_ = false;

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ExtensionsList = options.ExtensionsList;

  /**
   * ExtensionSettings class
   * Encapsulated handling of the 'Manage Extensions' page.
   * @class
   */
  function ExtensionSettings() {
    OptionsPage.call(this,
                     'extensions',
                     templateData.extensionSettingsTitle,
                     'extension-settings');
  }

  cr.addSingletonGetter(ExtensionSettings);

  ExtensionSettings.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionSettingsRequestExtensionsData');

      // Set up the developer mode button.
      var toggleDevMode = $('toggle-dev-on');
      toggleDevMode.addEventListener('click',
          this.handleToggleDevMode_.bind(this));

      // Setup the gallery related links and text.
      $('suggest-gallery').innerHTML =
          localStrings.getString('extensionSettingsSuggestGallery');
      $('get-more-extensions').innerHTML =
          localStrings.getString('extensionSettingsGetMoreExtensions');

      // Set up the three dev mode buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click',
          this.handleLoadUnpackedExtension_.bind(this));
      $('pack-extension').addEventListener('click',
          this.handlePackExtension_.bind(this));
      $('update-extensions-now').addEventListener('click',
          this.handleUpdateExtensionNow_.bind(this));
    },

    /**
     * Utility function which asks the C++ to show a platform-specific file
     * select dialog, and fire |callback| with the |filePath| that resulted.
     * |selectType| can be either 'file' or 'folder'. |operation| can be 'load',
     * 'packRoot', or 'pem' which are signals to the C++ to do some
     * operation-specific configuration.
     * @private
     */
    showFileDialog_: function(selectType, operation, callback) {
      handleFilePathSelected = function(filePath) {
        callback(filePath);
        handleFilePathSelected = function() {};
      };

      chrome.send('extensionSettingsSelectFilePath', [selectType, operation]);
    },

    /**
     * Handles the Load Unpacked Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handleLoadUnpackedExtension_: function(e) {
      this.showFileDialog_('folder', 'load', function(filePath) {
        chrome.send('extensionSettingsLoad', [String(filePath)]);
      });

      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_LoadUnpackedExtension']);
    },

    /**
     * Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackExtension_: function(e) {
      OptionsPage.navigateToPage('packExtensionOverlay');
      chrome.send('coreOptionsUserMetricsAction', ['Options_PackExtension']);
    },

    /**
     * Handles the Update Extension Now button.
     * @param {Event} e Change event.
     * @private
     */
    handleUpdateExtensionNow_: function(e) {
      chrome.send('extensionSettingsAutoupdate', []);
    },

    /**
     * Handles the Toggle Dev Mode button.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleDevMode_: function(e) {
      var dev = $('dev');
      if (!dev.classList.contains('dev-open')) {
        // Make the Dev section visible.
        dev.classList.add('dev-open');
        dev.classList.remove('dev-closed');

        $('load-unpacked').classList.add('dev-button-visible');
        $('load-unpacked').classList.remove('dev-button-hidden');
        $('pack-extension').classList.add('dev-button-visible');
        $('pack-extension').classList.remove('dev-button-hidden');
        $('update-extensions-now').classList.add('dev-button-visible');
        $('update-extensions-now').classList.remove('dev-button-hidden');
      } else {
        // Hide the Dev section.
        dev.classList.add('dev-closed');
        dev.classList.remove('dev-open');

        $('load-unpacked').classList.add('dev-button-hidden');
        $('load-unpacked').classList.remove('dev-button-visible');
        $('pack-extension').classList.add('dev-button-hidden');
        $('pack-extension').classList.remove('dev-button-visible');
        $('update-extensions-now').classList.add('dev-button-hidden');
        $('update-extensions-now').classList.remove('dev-button-visible');
      }

      chrome.send('extensionSettingsToggleDeveloperMode', []);
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of installed extensions.
   */
  ExtensionSettings.returnExtensionsData = function(extensionsData) {
    webui_responded_ = true;

    $('no-extensions').hidden = true;
    $('suggest-gallery').hidden = true;
    $('get-more-extensions-container').hidden = true;

    if (extensionsData.extensions.length > 0) {
      // Enforce order specified in the data or (if equal) then sort by
      // extension name (case-insensitive).
      extensionsData.extensions.sort(function(a, b) {
        if (a.order == b.order) {
          a = a.name.toLowerCase();
          b = b.name.toLowerCase();
          return a < b ? -1 : (a > b ? 1 : 0);
        } else {
          return a.order < b.order ? -1 : 1;
        }
      });

      $('get-more-extensions-container').hidden = false;
    } else {
      $('no-extensions').hidden = false;
      $('suggest-gallery').hidden = false;
    }

    ExtensionsList.prototype.data_ = extensionsData;

    var extensionList = $('extension-settings-list');
    ExtensionsList.decorate(extensionList);
  }

  // Export
  return {
    ExtensionSettings: ExtensionSettings
  };
});
