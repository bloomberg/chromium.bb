// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * AddStartupPageOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AddStartupPageOverlay() {
    OptionsPage.call(this, 'addStartupPageOverlay',
                     templateData.addStartupPageTitle,
                     'addStartupPageOverlay');
  }

  cr.addSingletonGetter(AddStartupPageOverlay);

  AddStartupPageOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('addStartupPageURL').focus();

      var self = this;
      var addForm = $('addStartupPageForm');
      addForm.onreset = cr.bind(this.dismissOverlay_, this);
      addForm.onsubmit =  function(e) {
        var urlField = $('addStartupPageURL');
        BrowserOptions.addStartupPage(urlField.value);

        self.dismissOverlay_();
        return false;
      };
      $('addStartupPageURL').oninput =
          cr.bind(this.updateAddButtonState_, this);
      $('addStartupPageURL').onkeydown = function(e) {
        if (e.keyCode == 27)  // Esc
          $('addStartupPageForm').reset();
      };
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      $('addStartupPageURL').value = '';
      this.updateAddButtonState_();
      OptionsPage.clearOverlays();
    },

    /**
     * Sets the enabled state of the startup page Add button based on
     * the current value of the text field.
     * @private
     */
    updateAddButtonState_: function() {
      $('addStartupPageAddButton').disabled =
          $('addStartupPageURL').value == '';
    },
  };

  // Export
  return {
    AddStartupPageOverlay: AddStartupPageOverlay
  };

});

