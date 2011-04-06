// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  // The GUID of the loaded address.
  var guid;

  /**
   * AutofillEditAddressOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutofillEditAddressOverlay() {
    OptionsPage.call(this, 'autofillEditAddress',
                     templateData.autofillEditAddressTitle,
                     'autofill-edit-address-overlay');
  }

  cr.addSingletonGetter(AutofillEditAddressOverlay);

  AutofillEditAddressOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createMultiValueLists_();

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
     * Creates, decorates and initializes the multi-value lists for full name,
     * phone, fax, and email.
     * @private
     */
    createMultiValueLists_: function() {
      var list = $('full-name-list');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;

      list = $('phone-list');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;

      list = $('fax-list');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;

      list = $('email-list');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the data model for the list named |listName| with the values from
     * |entries|.
     * @param {String} listName The id of the list.
     * @param {Array} entries The list of items to be added to the list.
     */
    setMultiValueList_: function(listName, entries) {
      // Add data entries, filtering null or empty strings.
      var list = $(listName);
      list.dataModel = new ArrayDataModel(
          entries.filter(function(i) {return i}));

      // Add special entry for adding new values.
      list.dataModel.splice(list.dataModel.length, 0, null);

      var self = this;
      list.dataModel.addEventListener(
        'splice', function(event) { self.inputFieldChanged_(); });
      list.dataModel.addEventListener(
        'change', function(event) { self.inputFieldChanged_(); });
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
     * array to the Autofill handler.
     * @private
     */
    saveAddress_: function() {
      var address = new Array();
      address[0] = this.guid;
      var list = $('full-name-list');
      address[1] = list.dataModel.slice(0, list.dataModel.length - 1);
      address[2] = $('company-name').value;
      address[3] = $('addr-line-1').value;
      address[4] = $('addr-line-2').value;
      address[5] = $('city').value;
      address[6] = $('state').value;
      address[7] = $('postal-code').value;
      address[8] = $('country').value;
      list = $('phone-list');
      address[9] = list.dataModel.slice(0, list.dataModel.length - 1);
      list = $('fax-list');
      address[10] = list.dataModel.slice(0, list.dataModel.length - 1);
      list = $('email-list');
      address[11] = list.dataModel.slice(0, list.dataModel.length - 1);

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
      $('company-name').oninput = $('addr-line-1').oninput =
      $('addr-line-2').oninput = $('city').oninput = $('state').oninput =
      $('postal-code').oninput = function(event) {
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
      // Length of lists are tested for <= 1 due to the "add" placeholder item
      // in the list.
      var disabled =
          $('full-name-list').items.length <= 1 &&
          !$('company-name').value &&
          !$('addr-line-1').value && !$('addr-line-2').value &&
          !$('city').value && !$('state').value && !$('postal-code').value &&
          !$('country').value && $('phone-list').items.length <= 1 &&
          $('fax-list').items.length <= 1 && $('email-list').items.length <= 1;
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
      this.setMultiValueList_('full-name-list', []);
      $('company-name').value = '';
      $('addr-line-1').value = '';
      $('addr-line-2').value = '';
      $('city').value = '';
      $('state').value = '';
      $('postal-code').value = '';
      $('country').value = '';
      this.setMultiValueList_('phone-list', []);
      this.setMultiValueList_('fax-list', []);
      this.setMultiValueList_('email-list', []);

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
      this.setMultiValueList_('full-name-list', address['fullName']);
      $('company-name').value = address['companyName'];
      $('addr-line-1').value = address['addrLine1'];
      $('addr-line-2').value = address['addrLine2'];
      $('city').value = address['city'];
      $('state').value = address['state'];
      $('postal-code').value = address['postalCode'];
      $('country').value = address['country'];
      this.setMultiValueList_('phone-list', address['phone']);
      this.setMultiValueList_('fax-list', address['fax']);
      this.setMultiValueList_('email-list', address['email']);

      this.countryChanged_();
    },
  };

  AutofillEditAddressOverlay.clearInputFields = function() {
    AutofillEditAddressOverlay.getInstance().clearInputFields_();
  };

  AutofillEditAddressOverlay.loadAddress = function(address) {
    AutofillEditAddressOverlay.getInstance().loadAddress_(address);
  };

  AutofillEditAddressOverlay.setTitle = function(title) {
    $('autofill-address-title').textContent = title;
  };

  // Export
  return {
    AutofillEditAddressOverlay: AutofillEditAddressOverlay
  };
});
