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
      $('autoFillEditAddressApplyButton').onclick = function(event) {
        self.saveAddress_();
        OptionsPage.clearOverlays();
      }

      self.clearInputFields_();
      self.connectInputEvents_();
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      OptionsPage.clearOverlays();
    },

    /**
     * Aggregates the values in the input fields into an associate array and
     * sends the array to the AutoFill handler.
     * @private
     */
    saveAddress_: function() {
      var address = new Array();
      address[0] = $('fullName').value;
      address[1] = $('companyName').value;
      address[2] = $('addrLine1').value;
      address[3] = $('addrLine2').value;
      address[4] = $('city').value;
      address[5] = $('state').value;
      address[6] = $('zipCode').value;
      address[7] = $('country').value;
      address[8] = $('phone').value;
      address[9] = $('fax').value;
      address[10] = $('email').value;
      chrome.send('addAddress', address);
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      var self = this;
      $('fullName').oninput = $('companyName').oninput =
      $('addrLine1').oninput = $('addrLine2').oninput = $('city').oninput =
      $('state').oninput = $('country').oninput = $('zipCode').oninput =
      $('phone').oninput = $('fax').oninput =
      $('email').oninput = function(event) {
        self.inputFieldChanged_();
      }
    },

    /**
     * Checks the values of each of the input fields and disables the 'Ok'
     * button if all of the fields are empty.
     * @private
     */
    inputFieldChanged_: function() {
      var disabled =
          !$('fullName').value && !$('companyName').value &&
          !$('addrLine1').value && !$('addrLine2').value && !$('city').value &&
          !$('state').value && !$('zipCode').value && !('country').value &&
          !$('phone').value && !$('fax').value && !$('email').value;
      $('autoFillEditAddressApplyButton').disabled = disabled;
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('fullName').value = '';
      $('companyName').value = '';
      $('addrLine1').value = '';
      $('addrLine2').value = '';
      $('city').value = '';
      $('state').value = '';
      $('zipCode').value = '';
      $('country').value = '';
      $('phone').value = '';
      $('fax').value = '';
      $('email').value = '';
    },
  };

  AutoFillEditAddressOverlay.clearInputFields = function() {
    AutoFillEditAddressOverlay.getInstance().clearInputFields_();
  };

  AutoFillEditAddressOverlay.setTitle = function(title) {
    $('autoFillAddressTitle').textContent = title;
  };

  // Export
  return {
    AutoFillEditAddressOverlay: AutoFillEditAddressOverlay
  };

});
