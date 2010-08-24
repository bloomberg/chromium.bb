// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

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

      self.setDefaultSelectOptions_();
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      AutoFillEditCreditCardOverlay.clearInputFields();
      OptionsPage.clearOverlays();
    },

    /**
     * Sets the default values of the options in the 'Billing address' and
     * 'Expiration date' select controls.
     * @private
     */
    setDefaultSelectOptions_: function() {
      // Set the 'Billing address' default option.
      var existingAddress = document.createElement('option');
      existingAddress.text = localStrings.getString('chooseExistingAddress');
      existingAddress.value = 'existingAddress';

      var billingAddress = $('billingAddress');
      billingAddress.add(existingAddress, null);

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
      var year = date.getFullYear();
      for (var i = 0; i < 10; ++i) {
        var text = parseInt(year) + i;
        var option = document.createElement('option');
        option.text = text;
        option.value = text;
        expirationYear.add(option, null);
      }
    }
  };

  AutoFillEditCreditCardOverlay.clearInputFields = function(title) {
    $('nameOnCard').value = '';
    $('billingAddress').value = '';
    $('creditCardNumber').value = '';
    $('expirationMonth').value = '';
    $('expirationYear').value = '';
  };

  AutoFillEditCreditCardOverlay.setTitle = function(title) {
    $('autoFillCreditCardTitle').textContent = title;
  };

  // Export
  return {
    AutoFillEditCreditCardOverlay: AutoFillEditCreditCardOverlay
  };

});
