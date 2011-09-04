// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * PackExtensionOverlay class
   * Encapsulated handling of the 'Pack Extension' overlay page.
   * @class
   */
  function PackExtensionOverlay() {
    OptionsPage.call(this, 'packExtensionOverlay',
                     templateData.packExtensionOverlayTabTitle,
                     'packExtensionOverlay');
  }

  cr.addSingletonGetter(PackExtensionOverlay);

  PackExtensionOverlay.prototype = {
    // Inherit PackExtensionOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('packExtensionDismiss').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
      $('packExtensionCommit').onclick = function(event) {
        var extensionPath = $('extensionRootDir').value;
        var privateKeyPath = $('extensionPrivateKey').value;
        chrome.send('pack', [extensionPath, privateKeyPath]);
      };
      $('browseExtensionDir').addEventListener('click',
          this.handleBrowseExtensionDir_.bind(this));
      $('browsePrivateKey').addEventListener('click',
          this.handleBrowsePrivateKey_.bind(this));
    },

    /**
    * Utility function which asks the C++ to show a platform-specific file
    * select dialog, and fire |callback| with the |filePath| that resulted.
    * |selectType| can be either 'file' or 'folder'. |operation| can be 'load',
    * 'packRoot', or 'pem' which are signals to the C++ to do some
    * operation-specific configuration.
    @private
    */
    showFileDialog_: function(selectType, operation, callback) {
      handleFilePathSelected = function(filePath) {
        callback(filePath);
        handleFilePathSelected = function() {};
      };

      chrome.send('extensionSettingsSelectFilePath', [selectType, operation]);
    },

    /**
     * Handles the showing of the extension directory browser.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowseExtensionDir_: function(e) {
      this.showFileDialog_('folder', 'load', function(filePath) {
        $('extensionRootDir').value = filePath;
      });
    },

    /**
     * Handles the showing of the extension private key file.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowsePrivateKey_: function(e) {
      this.showFileDialog_('file', 'load', function(filePath) {
        $('extensionPrivateKey').value = filePath;
      });
    },
  };

  // Export
  return {
    PackExtensionOverlay: PackExtensionOverlay
  };
});
