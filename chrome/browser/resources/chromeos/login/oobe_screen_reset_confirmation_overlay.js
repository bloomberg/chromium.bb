// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('reset', function() {

  /**
   * ResetScreenConfirmationOverlay class
   * Encapsulated handling of the 'Confirm reset device' overlay OOBE page.
   * @class
   */
  function ConfirmResetOverlay() {
  }

  cr.addSingletonGetter(ConfirmResetOverlay);

  ConfirmResetOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('reset-confirm-overlay');
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('reset-confirm-dismiss').addEventListener('click', this.handleDismiss_);
      $('reset-confirm-commit').addEventListener('click', this.handleCommit_);

      $('overlay-reset').removeAttribute('hidden');
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     */
    handleDismiss_: function(e) {
      $('reset').isConfirmational = false;
      $('overlay-reset').setAttribute('hidden', true);
      e.stopPropagation();
    },

    /**
     * Handles a click on the commit button.
     * @param {Event} e The click event.
     */
    handleCommit_: function(e) {
      $('reset').isConfirmational = false;
      chrome.send('powerwashOnReset', [$('reset').rollbackChecked]);
      $('overlay-reset').setAttribute('hidden', true);
      e.stopPropagation();
    },
  };

  // Export
  return {
    ConfirmResetOverlay: ConfirmResetOverlay
  };
});
