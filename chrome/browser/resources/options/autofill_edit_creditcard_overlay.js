// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  // The GUID of the loaded credit card.
  var guid_;

  // The CC number of the profile, used to check for changes to the input field.
  var storedCCNumber_;

  // Set to true if the user has edited the CC number field. When saving the
  // CC profile after editing, the stored CC number is saved if the input field
  // has not been modified.
  var hasEditedNumber_;

  /**
   * AutoFillEditCreditCardOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutoFillEditCreditCardOverlay() {
    OptionsPage.call(this, 'autoFillEditCreditCard',
                     templateData.autoFillEditCreditCardTitle,
                     'autofill-edit-credit-card-overlay');
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
      $('autofill-edit-credit-card-cancel-button').onclick = function(event) {
        self.dismissOverlay_();
      }
      $('autofill-edit-credit-card-apply-button').onclick = function(event) {
        self.saveCreditCard_();
        self.dismissOverlay_();
      }

      self.guid_ = '';
      self.storedCCNumber_ = '';
      self.hasEditedNumber_ = false;
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
      this.guid_ = '';
      this.storedCCNumber_ = '';
      this.hasEditedNumber_ = false;
      OptionsPage.closeOverlay();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the AutoFill handler.
     * @private
     */
    saveCreditCard_: function() {
      var creditCard = new Array(5);
      creditCard[0] = this.guid_;
      creditCard[1] = $('name-on-card').value;
      creditCard[3] = $('expiration-month').value;
      creditCard[4] = $('expiration-year').value;

      if (this.hasEditedNumber_)
        creditCard[2] = $('credit-card-number').value;
      else
        creditCard[2] = this.storedCCNumber_;

      chrome.send('setCreditCard', creditCard);
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      $('name-on-card').oninput = $('credit-card-number').oninput =
          $('expiration-month').onchange = $('expiration-year').onchange =
              this.inputFieldChanged_.bind(this);
    },

    /**
     * Checks the values of each of the input fields and disables the 'Ok'
     * button if all of the fields are empty.
     * @param {Event} opt_event Optional data for the 'input' event.
     * @private
     */
    inputFieldChanged_: function(opt_event) {
      var ccNumber = $('credit-card-number');
      var disabled = !$('name-on-card').value && !ccNumber.value;
      $('autofill-edit-credit-card-apply-button').disabled = disabled;

      if (opt_event && opt_event.target == ccNumber) {
        // If the user hasn't edited the text yet, delete it all on edit.
        if (!this.hasEditedNumber_ && this.storedCCNumber_.length &&
            ccNumber.value != this.storedCCNumber_) {
          ccNumber.value = '';
        }

        this.hasEditedNumber_ = true;
      }
    },

    /**
     * Sets the default values of the options in the 'Expiration date' select
     * controls.
     * @private
     */
    setDefaultSelectOptions_: function() {
      // Set the 'Expiration month' default options.
      var expirationMonth = $('expiration-month');
      expirationMonth.options.length = 0;
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
      var expirationYear = $('expiration-year');
      expirationYear.options.length = 0;

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
      $('name-on-card').value = '';
      $('credit-card-number').value = '';
      $('expiration-month').selectedIndex = 0;
      $('expiration-year').selectedIndex = 0;
    },

    /**
     * Sets the value of each input field according to |creditCard|
     * @private
     */
    setInputFields_: function(creditCard) {
      $('name-on-card').value = creditCard['nameOnCard'];
      $('credit-card-number').value = creditCard['obfuscatedCardNumber'];

      // The options for the year select control may be out-dated at this point,
      // e.g. the user opened the options page before midnight on New Year's Eve
      // and then loaded a credit card profile to edit in the new year, so
      // reload the select options just to be safe.
      this.setDefaultSelectOptions_();

      var idx = parseInt(creditCard['expirationMonth'], 10);
      $('expiration-month').selectedIndex = idx - 1;

      expYear = creditCard['expirationYear'];
      var date = new Date();
      var year = parseInt(date.getFullYear());
      for (var i = 0; i < 10; ++i) {
        var text = year + i;
        if (expYear == String(text))
          $('expiration-year').selectedIndex = i;
      }
    },

    /**
     * Loads the credit card data from |creditCard|, sets the input fields based
     * on this data and stores the GUID of the credit card.
     * @private
     */
    loadCreditCard_: function(creditCard) {
      this.setInputFields_(creditCard);
      this.inputFieldChanged_();
      this.guid_ = creditCard['guid'];
      this.storedCCNumber_ = creditCard['creditCardNumber'];
    },
  };

  AutoFillEditCreditCardOverlay.clearInputFields = function(title) {
    AutoFillEditCreditCardOverlay.getInstance().clearInputFields_();
  };

  AutoFillEditCreditCardOverlay.loadCreditCard = function(creditCard) {
    AutoFillEditCreditCardOverlay.getInstance().loadCreditCard_(creditCard);
  };

  AutoFillEditCreditCardOverlay.setTitle = function(title) {
    $('autofill-credit-card-title').textContent = title;
  };

  // Export
  return {
    AutoFillEditCreditCardOverlay: AutoFillEditCreditCardOverlay
  };
});
