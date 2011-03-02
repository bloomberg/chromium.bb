// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'autoFillEditAddress',
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
      self.populateCountryList_();
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
      address[7] = $('postal-code').value;
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
      $('state').oninput = $('postal-code').oninput = $('phone').oninput =
      $('fax').oninput = $('email').oninput = function(event) {
        self.inputFieldChanged_();
      }

      $('country').onchange = function(event) {
        self.countryChanged_();
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
          !$('city').value && !$('state').value && !$('postal-code').value &&
          !$('country').value && !$('phone').value && !$('fax').value &&
          !$('email').value;
      $('autofill-edit-address-apply-button').disabled = disabled;
    },

    /**
     * Updates the postal code and state field labels appropriately for the
     * selected country.
     * @private
     */
    countryChanged_: function() {
      var countryCode = $('country').value;
      if (!countryCode)
        countryCode = templateData.defaultCountryCode;

      var details = templateData.autofillCountryData[countryCode];
      var postal = $('postal-code-label');
      postal.textContent = details['postalCodeLabel'];
      $('state-label').textContent = details['stateLabel'];

      // Also update the 'Ok' button as needed.
      this.inputFieldChanged_();
    },

    /**
     * Populates the country <select> list.
     * @private
     */
    populateCountryList_: function() {
      var countryData = templateData.autofillCountryData;
      var defaultCountryCode = templateData.defaultCountryCode;

      // Build an array of the country names and their corresponding country
      // codes, so that we can sort and insert them in order.
      var countries = [];
      for (var countryCode in countryData) {
        // We always want the default country to be at the top of the list, so
        // we handle it separately.
        if (countryCode == defaultCountryCode)
          continue;

        var country = {
          countryCode: countryCode,
          name: countryData[countryCode]['name']
        };
        countries.push(country);
      }

      // Sort the countries in alphabetical order by name.
      countries = countries.sort(function(a, b) {
        return a.name < b.name ? -1 : 1;
      });

      // Insert the empty and default countries at the beginning of the array.
      var emptyCountry = {
        countryCode: '',
        name: ''
      };
      var defaultCountry = {
        countryCode: defaultCountryCode,
        name: countryData[defaultCountryCode]['name']
      };
      countries.unshift(emptyCountry, defaultCountry);

      // Add the countries to the country <select> list.
      var countryList = $('country');
      for (var i = 0; i < countries.length; i++) {
        var country = new Option(countries[i].name, countries[i].countryCode);
        countryList.appendChild(country)
      }
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
      $('postal-code').value = '';
      $('country').value = '';
      $('phone').value = '';
      $('fax').value = '';
      $('email').value = '';

      this.countryChanged_();
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
      $('postal-code').value = address['postalCode'];
      $('country').value = address['country'];
      $('phone').value = address['phone'];
      $('fax').value = address['fax'];
      $('email').value = address['email'];

      this.countryChanged_();
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
