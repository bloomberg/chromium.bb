// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('apps_dev_tool', function() {
  'use strict';

/** const*/ var AppsDevTool = apps_dev_tool.AppsDevTool;

  /**
   * Hides the present overlay showing.
   */
  var hideOverlay = function() {
    AppsDevTool.showOverlay(null);
  };

  /**
   * PackItemOverlay class
   * Encapsulated handling of the 'Pack Item' overlay page.
   * @constructor
   */
  function PackItemOverlay() {}

  cr.addSingletonGetter(PackItemOverlay);

  PackItemOverlay.prototype = {
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      overlay.addEventListener('cancelOverlay', hideOverlay.bind(this));

      $('pack-item-dismiss').addEventListener('click',
          hideOverlay.bind(this));
      $('pack-item-commit').addEventListener('click',
          this.handleCommit_.bind(this));
      $('browse-item-dir').addEventListener('click',
          this.handleBrowseItemDir_.bind(this));
      $('browse-private-key').addEventListener('click',
          this.handleBrowsePrivateKey_.bind(this));
    },

    /**
     * Handles a click on the pack button.
     * @param {Event} e The click event.
     * @private
     */
    handleCommit_: function(e) {
      var itemPath = $('item-root-dir').value;
      var privateKeyPath = $('item-private-key').value;
      chrome.developerPrivate.packDirectory(
          itemPath, privateKeyPath, 0, this.onCommit_);
    },

    /**
     * Handles a commit on the pack request.
     * @param {string} response Message returned by packing api.
     * @private
     */
    onCommit_: function(response) {
      if (response.status == 'SUCCESS')
        PackItemOverlay.showSuccessMessage(response);
      else if (response.status == 'ERROR')
        PackItemOverlay.showError(response);
      else
        PackItemOverlay.showWarningMessage(response);
    },

    /**
     * Handles the showing of the item directory browser.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowseItemDir_: function(e) {
      chrome.developerPrivate.choosePath('FOLDER', 'LOAD', function(filePath) {
        $('item-root-dir').value = filePath;
      });
    },

    /**
     * Handles the showing of the item private key file.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowsePrivateKey_: function(e) {
      chrome.developerPrivate.choosePath('FILE', 'PEM', function(filePath) {
        $('item-private-key').value = filePath;
      });
    },
  };

  /**
   * Wrap up the pack process by showing the success |message| and closing
   * the overlay.
   * @param {string} message The message to show to the user.
   */
  PackItemOverlay.showSuccessMessage = function(response) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionOverlay'),
        response.message,
        loadTimeData.getString('ok'),
        '',
        hideOverlay,
        null);
    AppsDevTool.showOverlay($('alertOverlay'));
  };

  /**
   * An alert overlay showing |message|, and upon acknowledgement, close
   * the alert overlay and return to showing the PackItemOverlay.
   * @param {string} message The message to show to the user.
   */
  PackItemOverlay.showError = function(response) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionErrorTitle'),
        response.message /* message returned by the packiing api */,
        loadTimeData.getString('ok'),
        '',
        function() {
          AppsDevTool.showOverlay($('packItemOverlay'));
        },
        null);
    AppsDevTool.showOverlay($('alertOverlay'));
  };

  /**
   * An alert overlay showing |message| as warning and proceeding after the
   * user confirms the action.
   * @param {response} response returned by the packItem API.
   */
  PackItemOverlay.showWarningMessage = function(response) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionWarningTitle'),
        response.message /* message returned by the packing api */,
        loadTimeData.getString('packExtensionProceedAnyway'),
        loadTimeData.getString('cancel'),
        function() {
          chrome.developerPrivate.packDirectory(
              response.item_path,
              response.pem_path,
              response.override_flags,
              PackItemOverlay.showSuccessMessage);
          hideOverlay();
        },
        hideOverlay);
    AppsDevTool.showOverlay($('alertOverlay'));
  };

  // Export
  return {
    PackItemOverlay: PackItemOverlay,
  };
});
