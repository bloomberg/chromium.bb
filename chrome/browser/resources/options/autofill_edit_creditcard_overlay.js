// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  // The unique ID of the loaded credit card.
  var uniqueID;

  // The unique IDs of the billing addresses.
  var billingAddressIDs;

  /**
   * AutoFillEditCreditCardOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutoFillEditCreditCardOverlay() {
    OptionsPage.call(this, 'autoFillEditCreditCardOverlay',
                     templateData.autoFillEditCreditCardTitle,
                     'autoFillEditCreditCardOverlay');
  }

  cr.addSingletonGetter(AutoFillEditCreditCardOverlay);

  AutoFillEditCreditCardOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('autoFillEditCreditCardCancelButton').onclick = function(event) {
        self.dismissOverlay_();
      }
      $('autoFillEditCreditCardApplyButton').onclick = function(event) {
        self.saveCreditCard_();
        self.dismissOverlay_();
      }

      self.uniqueID = 0;
      self.billingAddressIDs = new Array();
      self.clearInputFields_();
      self.connectInputEvents_();
      self.setDefaultSelectOptions_();
    },

    /**
     * Clears any uncommitted input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      this.uniqueID = 0;
      OptionsPage.clearOverlays();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the AutoFill handler.
     * @private
     */
    saveCreditCard_: function() {
      var creditCard = new Array(6);
      creditCard[0] = String(this.uniqueID);
      creditCard[1] = $('nameOnCard').value;
      creditCard[2] = '0';
      creditCard[3] = $('creditCardNumber').value;
      creditCard[4] = $('expirationMonth').value;
      creditCard[5] = $('expirationYear').value;

      // Set the ID if available.
      if (this.billingAddressIDs.length != 0) {
        creditCard[2] =
            String(this.billingAddressIDs[$('billingAddress').selectedIndex]);
      }

      chrome.send('updateCreditCard', creditCard);
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      var self = this;
      $('nameOnCard').oninput = $('billingAddress').onchange =
      $('creditCardNumber').oninput = $('expirationMonth').onchange =
      $('expirationYear').onchange = function(event) {
        self.inputFieldChanged_();
      }
    },

    /**
     * Checks the values of each of the input fields and disables the 'Ok'
     * button if all of the fields are empty.
     * @private
     */
    inputFieldChanged_: function() {
      var disabled = !$('nameOnCard').value && !$('creditCardNumber').value;
      $('autoFillEditCreditCardApplyButton').disabled = disabled;
    },

    /**
     * Clears the options from the billing address select control.
     * @private
     */
    clearBillingAddressControl_: function() {
      $('billingAddress').length = 0;
    },

    /**
     * Sets the default billing address in the 'Billing address' select control.
     * @private
     */
    setDefaultBillingAddress_: function() {
      this.clearBillingAddressControl_();

      var existingAddress =
          new Option(localStrings.getString('chooseExistingAddress'));
      $('billingAddress').add(existingAddress, null);
    },

    /**
     * Sets the default values of the options in the 'Billing address' and
     * 'Expiration date' select controls.
     * @private
     */
    setDefaultSelectOptions_: function() {
      this.setDefaultBillingAddress_();

      // Set the 'Expiration month' default options.
      var expirationMonth = $('expirationMonth');
      for (var i = 1; i <= 12; ++i) {
        var text;
        if (i < 10)
          text = '0' + i;
        else
          text = i;

        var option = document.createElement('option');
        option.text = text;
        option.value = text;
        expirationMonth.add(option, null);
      }

      // Set the 'Expiration year' default options.
      var expirationYear = $('expirationYear');
      var date = new Date();
      var year = parseInt(date.getFullYear());
      for (var i = 0; i < 10; ++i) {
        var text = year + i;
        var option = document.createElement('option');
        option.text = text;
        option.value = text;
        expirationYear.add(option, null);
      }
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('nameOnCard').value = '';
      $('billingAddress').selectedIndex = 0;
      $('creditCardNumber').value = '';
      $('expirationMonth').selectedIndex = 0;
      $('expirationYear').selectedIndex = 0;
    },

    /**
     * Sets the value of each input field according to |creditCard|
     * @private
     */
    setInputFields_: function(creditCard) {
      $('nameOnCard').value = creditCard['nameOnCard'];
      $('creditCardNumber').value = creditCard['creditCardNumber'];

      // The options for the year select control may be out-dated at this point,
      // e.g. the user opened the options page before midnight on New Year's Eve
      // and then loaded a credit card profile to edit in the new year, so
      // reload the select options just to be safe.
      this.setDefaultSelectOptions_();

      var id = parseInt(creditCard['billingAddress']);
      for (var i = 0; i < this.billingAddressIDs.length; ++i) {
        if (this.billingAddressIDs[i] == id)
          $('billingAddress').selectedIndex = i;
      }

      var idx = parseInt(creditCard['expirationMonth'], 10);
      $('expirationMonth').selectedIndex = idx - 1;

      expYear = creditCard['expirationYear'];
      var date = new Date();
      var year = parseInt(date.getFullYear());
      for (var i = 0; i < 10; ++i) {
        var text = year + i;
        if (expYear == String(text))
          $('expirationYear').selectedIndex = i;
      }
    },

    /**
     * Loads the credit card data from |creditCard|, sets the input fields based
     * on this data and stores the unique ID of the credit card.
     * @private
     */
    loadCreditCard_: function(creditCard) {
      this.setInputFields_(creditCard);
      this.inputFieldChanged_();
      this.uniqueID = creditCard['uniqueID'];
    },

    /**
     * Sets the 'billingAddress' select control with the address labels in
     * |addresses|.  Also stores the unique IDs of the corresponding addresses
     * on this object.
     * @private
     */
    setBillingAddresses_: function(addresses) {
      this.billingAddressIDs = new Array(addresses.length);

      if (addresses.length == 0) {
        this.setDefaultBillingAddress_();
        return;
      }

      this.clearBillingAddressControl_();

      for (var i = 0; i < addresses.length; ++i) {
        var address = addresses[i];
        var option = new Option(address['label']);
        this.billingAddressIDs[i] = address['uniqueID'];
        billingAddress.add(option, null);
      }
    },
  };

  AutoFillEditCreditCardOverlay.clearInputFields = function(title) {
    AutoFillEditCreditCardOverlay.getInstance().clearInputFields_();
  };

  AutoFillEditCreditCardOverlay.loadCreditCard = function(creditCard) {
    AutoFillEditCreditCardOverlay.getInstance().loadCreditCard_(creditCard);
  };

  AutoFillEditCreditCardOverlay.setTitle = function(title) {
    $('autoFillCreditCardTitle').textContent = title;
  };

  AutoFillEditCreditCardOverlay.setBillingAddresses = function(addresses) {
    AutoFillEditCreditCardOverlay.getInstance().setBillingAddresses_(addresses);
  };

  // Export
  return {
    AutoFillEditCreditCardOverlay: AutoFillEditCreditCardOverlay
  };

});
