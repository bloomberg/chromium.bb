// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
      $('creditCardNumber').onkeydown = this.onKeyDown_.bind(this);

      self.guid_ = '';
      self.storedCCNumber_ = '';
      self.hasEditedNumber_ = false;
      self.clearInputFields_();
      self.connectInputEvents_();
      self.setDefaultSelectOptions_();
    },

    /**
     * Handles the keydown event.
     * @private
     */
    onKeyDown_: function(event) {
      // If the user hasn't edited the text yet, delete it all on edit.
      if (!this.hasEditedNumber_ &&
          $('creditCardNumber').value != this.storedCCNumber_) {
        this.hasEditedNumber_ = true;
        $('creditCardNumber').value = '';
      }
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
      OptionsPage.clearOverlays();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the AutoFill handler.
     * @private
     */
    saveCreditCard_: function() {
      var creditCard = new Array(5);
      creditCard[0] = this.guid_;
      creditCard[1] = $('nameOnCard').value;
      creditCard[3] = $('expirationMonth').value;
      creditCard[4] = $('expirationYear').value;

      if (this.hasEditedNumber_)
        creditCard[2] = $('creditCardNumber').value;
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
      var self = this;
      $('nameOnCard').oninput = $('creditCardNumber').oninput =
          $('expirationMonth').onchange = $('expirationYear').onchange =
              function(event) {
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
     * Sets the default values of the options in the 'Expiration date' select
     * controls.
     * @private
     */
    setDefaultSelectOptions_: function() {
      // Set the 'Expiration month' default options.
      var expirationMonth = $('expirationMonth');
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
      var expirationYear = $('expirationYear');
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
      $('nameOnCard').value = '';
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
      $('creditCardNumber').value = creditCard['obfuscatedCardNumber'];

      // The options for the year select control may be out-dated at this point,
      // e.g. the user opened the options page before midnight on New Year's Eve
      // and then loaded a credit card profile to edit in the new year, so
      // reload the select options just to be safe.
      this.setDefaultSelectOptions_();

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
    $('autoFillCreditCardTitle').textContent = title;
  };

  // Export
  return {
    AutoFillEditCreditCardOverlay: AutoFillEditCreditCardOverlay
  };

});
