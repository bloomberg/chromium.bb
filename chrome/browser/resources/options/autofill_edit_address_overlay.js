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
                     'autofill-edit-address-overlay');
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
      $('autofill-edit-address-cancel-button').onclick = function(event) {
        self.dismissOverlay_();
      }
      $('autofill-edit-address-apply-button').onclick = function(event) {
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
      address[1] = $('full-name').value;
      address[2] = $('company-name').value;
      address[3] = $('addr-line-1').value;
      address[4] = $('addr-line-2').value;
      address[5] = $('city').value;
      address[6] = $('state').value;
      address[7] = $('zip-code').value;
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
      $('full-name').oninput = $('company-name').oninput =
      $('addr-line-1').oninput = $('addr-line-2').oninput = $('city').oninput =
      $('state').oninput = $('country').oninput = $('zip-code').oninput =
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
          !$('full-name').value && !$('company-name').value &&
          !$('addr-line-1').value && !$('addr-line-2').value &&
          !$('city').value && !$('state').value && !$('zip-code').value &&
          !$('country').value && !$('phone').value && !$('fax').value &&
          !$('email').value;
      $('autofill-edit-address-apply-button').disabled = disabled;
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('full-name').value = '';
      $('company-name').value = '';
      $('addr-line-1').value = '';
      $('addr-line-2').value = '';
      $('city').value = '';
      $('state').value = '';
      $('zip-code').value = '';
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
      $('full-name').value = address['fullName'];
      $('company-name').value = address['companyName'];
      $('addr-line-1').value = address['addrLine1'];
      $('addr-line-2').value = address['addrLine2'];
      $('city').value = address['city'];
      $('state').value = address['state'];
      $('zip-code').value = address['zipCode'];
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
    $('autofill-address-title').textContent = title;
  };

  // Export
  return {
    AutoFillEditAddressOverlay: AutoFillEditAddressOverlay
  };
});
