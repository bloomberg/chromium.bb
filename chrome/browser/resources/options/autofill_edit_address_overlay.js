// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * AutoFillEditAddressOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutoFillEditAddressOverlay() {
    OptionsPage.call(this, 'autoFillEditAddressOverlay',
                     templateData.autoFillEditAddressTitle,
                     'autoFillEditAddressOverlay');
  }

  cr.addSingletonGetter(AutoFillEditAddressOverlay);

  AutoFillEditAddressOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('autoFillEditAddressCancelButton').onclick = function(event) {
        self.dismissOverlay_();
      }
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      $('fullName').value = '';
      $('companyName').value = '';
      $('addrLine1').value = '';
      $('addrLine2').value = '';
      $('city').value = '';
      $('state').value = '';
      $('zipCode').value = '';
      $('phone').value = '';
      $('fax').value = '';
      $('email').value = '';
      OptionsPage.clearOverlays();
    },

  };

  // Export
  return {
    AutoFillEditAddressOverlay: AutoFillEditAddressOverlay
  };

});
