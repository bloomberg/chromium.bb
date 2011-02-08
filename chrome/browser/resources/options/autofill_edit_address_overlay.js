// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  // The GUID of the loaded address.
  var guid;

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
        self.dismissOverlay_();
      }

      self.guid = '';
      self.clearInputFields_();
      self.connectInputEvents_();
    },

    /**
     * Clears any uncommitted input, resets the stored GUID and dismisses the
     * overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      this.guid = '';
      OptionsPage.closeOverlay();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the AutoFill handler.
     * @private
     */
    saveAddress_: function() {
      var address = new Array();
      address[0] = this.guid;
      address[1] = $('fullName').value;
      address[2] = $('companyName').value;
      address[3] = $('addrLine1').value;
      address[4] = $('addrLine2').value;
      address[5] = $('city').value;
      address[6] = $('state').value;
      address[7] = $('zipCode').value;
      address[8] = $('country').value;
      address[9] = $('phone').value;
      address[10] = $('fax').value;
      address[11] = $('email').value;

      chrome.send('setAddress', address);
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

    /**
     * Loads the address data from |address|, sets the input fields based on
     * this data and stores the GUID of the address.
     * @private
     */
    loadAddress_: function(address) {
      this.setInputFields_(address);
      this.inputFieldChanged_();
      this.guid = address['guid'];
    },

    /**
     * Sets the value of each input field according to |address|
     * @private
     */
    setInputFields_: function(address) {
      $('fullName').value = address['fullName'];
      $('companyName').value = address['companyName'];
      $('addrLine1').value = address['addrLine1'];
      $('addrLine2').value = address['addrLine2'];
      $('city').value = address['city'];
      $('state').value = address['state'];
      $('zipCode').value = address['zipCode'];
      $('country').value = address['country'];
      $('phone').value = address['phone'];
      $('fax').value = address['fax'];
      $('email').value = address['email'];
    },
  };

  AutoFillEditAddressOverlay.clearInputFields = function() {
    AutoFillEditAddressOverlay.getInstance().clearInputFields_();
  };

  AutoFillEditAddressOverlay.loadAddress = function(address) {
    AutoFillEditAddressOverlay.getInstance().loadAddress_(address);
  };

  AutoFillEditAddressOverlay.setTitle = function(title) {
    $('autoFillAddressTitle').textContent = title;
  };

  // Export
  return {
    AutoFillEditAddressOverlay: AutoFillEditAddressOverlay
  };
});
