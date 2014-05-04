// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  // The GUID of the loaded address.
  var guid;

  // The BCP 47 language code for the layout of input fields.
  var languageCode;

  /**
   * AutofillEditAddressOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutofillEditAddressOverlay() {
    OptionsPage.call(this, 'autofillEditAddress',
                     loadTimeData.getString('autofillEditAddressTitle'),
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
      };

      // TODO(jhawkins): Investigate other possible solutions.
      $('autofill-edit-address-apply-button').onclick = function(event) {
        // Blur active element to ensure that pending changes are committed.
        if (document.activeElement)
          document.activeElement.blur();
        // Blurring is delayed for list elements.  Queue save and close to
        // ensure that pending changes have been applied.
        setTimeout(function() {
          self.pageDiv.querySelector('[field=phone]').doneValidating().then(
              function() {
                self.saveAddress_();
                self.dismissOverlay_();
              });
        }, 0);
      };

      // Prevent 'blur' events on the OK and cancel buttons, which can trigger
      // insertion of new placeholder elements.  The addition of placeholders
      // affects layout, which interferes with being able to click on the
      // buttons.
      $('autofill-edit-address-apply-button').onmousedown = function(event) {
        event.preventDefault();
      };
      $('autofill-edit-address-cancel-button').onmousedown = function(event) {
        event.preventDefault();
      };

      this.guid = '';
      this.populateCountryList_();
      this.rebuildInputFields_(
          loadTimeData.getValue('autofillDefaultCountryComponents'));
      this.languageCode =
          loadTimeData.getString('autofillDefaultCountryLanguageCode');
      this.connectInputEvents_();
      this.setInputFields_({});
      this.getCountrySelector_().onchange = function(event) {
        self.countryChanged_();
      };
    },

    /**
    * Specifically catch the situations in which the overlay is cancelled
    * externally (e.g. by pressing <Esc>), so that the input fields and
    * GUID can be properly cleared.
    * @override
    */
    handleCancel: function() {
      this.dismissOverlay_();
    },

    /**
     * Creates, decorates and initializes the multi-value lists for phone and
     * email.
     * @private
     */
    createMultiValueLists_: function() {
      var list = this.pageDiv.querySelector('[field=phone]');
      options.autofillOptions.AutofillPhoneValuesList.decorate(list);
      list.autoExpands = true;

      list = this.pageDiv.querySelector('[field=email]');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the data model for the |list| with the values from |entries|.
     * @param {element} list The list to update.
     * @param {Array} entries The list of items to be added to the list.
     * @private
     */
    setMultiValueList_: function(list, entries) {
      // Add special entry for adding new values.
      var augmentedList = entries.slice();
      augmentedList.push(null);
      list.dataModel = new ArrayDataModel(augmentedList);

      // Update the status of the 'OK' button.
      this.inputFieldChanged_();

      list.dataModel.addEventListener('splice',
                                      this.inputFieldChanged_.bind(this));
      list.dataModel.addEventListener('change',
                                      this.inputFieldChanged_.bind(this));
    },

    /**
     * Clears any uncommitted input, resets the stored GUID and dismisses the
     * overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.setInputFields_({});
      this.inputFieldChanged_();
      this.guid = '';
      this.languageCode = '';
      OptionsPage.closeOverlay();
    },

    /**
     * Returns the country selector element.
     * @return {element} The country selector.
     * @private
     */
    getCountrySelector_: function() {
      return this.pageDiv.querySelector('[field=country]');
    },

    /**
     * Returns all list elements.
     * @return {NodeList} The list elements.
     * @private
     */
    getLists_: function() {
      return this.pageDiv.querySelectorAll('list[field]');
    },

    /**
     * Returns all text input elements.
     * @return {NodeList} The text input elements.
     * @private
     */
    getTextFields_: function() {
      return this.pageDiv.querySelectorAll(
          ':-webkit-any(textarea, input)[field]');
    },

    /**
     * Aggregates the values in the input fields into an object.
     * @return {object} The mapping from field names to values.
     * @private
     */
    getInputFields_: function() {
      var address = {};
      address['country'] = this.getCountrySelector_().value;

      var lists = this.getLists_();
      for (var i = 0; i < lists.length; i++) {
        address[lists[i].getAttribute('field')] =
            lists[i].dataModel.slice(0, lists[i].dataModel.length - 1);
      }

      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        address[fields[i].getAttribute('field')] = fields[i].value;
      }

      return address;
    },

    /**
     * Sets the value of each input field according to |address|.
     * @param {object} address The object with values to use.
     * @private
     */
    setInputFields_: function(address) {
      this.getCountrySelector_().value = address['country'] || '';

      var lists = this.getLists_();
      for (var i = 0; i < lists.length; i++) {
        this.setMultiValueList_(
            lists[i], address[lists[i].getAttribute('field')] || []);
      }

      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        fields[i].value = address[fields[i].getAttribute('field')] || '';
      }
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the Autofill handler.
     * @private
     */
    saveAddress_: function() {
      var inputFields = this.getInputFields_();
      var address = new Array();
      var argCounter = 0;
      address[argCounter++] = this.guid;
      address[argCounter++] = inputFields['fullName'] || [];
      address[argCounter++] = inputFields['companyName'] || '';
      address[argCounter++] = inputFields['addrLines'] || '';
      address[argCounter++] = inputFields['dependentLocality'] || '';
      address[argCounter++] = inputFields['city'] || '';
      address[argCounter++] = inputFields['state'] || '';
      address[argCounter++] = inputFields['postalCode'] || '';
      address[argCounter++] = inputFields['sortingCode'] || '';
      address[argCounter++] = inputFields['country'] || '';
      address[argCounter++] = inputFields['phone'] || [];
      address[argCounter++] = inputFields['email'] || [];
      address[argCounter++] = this.languageCode;

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
      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        fields[i].oninput = function(event) { self.inputFieldChanged_(); };
      }
    },

    /**
     * Disables the 'Ok' button if all of the fields are empty.
     * @private
     */
    inputFieldChanged_: function() {
      var disabled = true;
      if (this.getCountrySelector_().value)
        disabled = false;

      if (disabled) {
        // Length of lists are tested for > 1 due to the "add" placeholder item
        // in the list.
        var lists = this.getLists_();
        for (var i = 0; i < lists.length; i++) {
          if (lists[i].items.length > 1) {
            disabled = false;
            break;
          }
        }
      }

      if (disabled) {
        var fields = this.getTextFields_();
        for (var i = 0; i < fields.length; i++) {
          if (fields[i].value) {
            disabled = false;
            break;
          }
        }
      }

      $('autofill-edit-address-apply-button').disabled = disabled;
    },

    /**
     * Updates the address fields appropriately for the selected country.
     * @private
     */
    countryChanged_: function() {
      var countryCode = this.getCountrySelector_().value;
      if (countryCode)
        chrome.send('loadAddressEditorComponents', [countryCode]);
      else
        this.inputFieldChanged_();
    },

    /**
     * Populates the country <select> list.
     * @private
     */
    populateCountryList_: function() {
      var countryList = loadTimeData.getValue('autofillCountrySelectList');

      // Add the countries to the country <select> list.
      var countrySelect = this.getCountrySelector_();
      // Add an empty option.
      countrySelect.appendChild(new Option('', ''));
      for (var i = 0; i < countryList.length; i++) {
        var option = new Option(countryList[i].name,
                                countryList[i].value);
        option.disabled = countryList[i].value == 'separator';
        countrySelect.appendChild(option);
      }
    },

    /**
     * Loads the address data from |address|, sets the input fields based on
     * this data, and stores the GUID and language code of the address.
     * @private
     */
    loadAddress_: function(address) {
      this.rebuildInputFields_(address.components);
      this.setInputFields_(address);
      this.inputFieldChanged_();
      this.connectInputEvents_();
      this.guid = address.guid;
      this.languageCode = address.languageCode;
    },

    /**
     * Takes a snapshot of the input values, clears the input values, loads the
     * address input layout from |input.components|, restores the input values
     * from snapshot, and stores the |input.languageCode| for the address.
     * @private
     */
    loadAddressComponents_: function(input) {
      var address = this.getInputFields_();
      this.rebuildInputFields_(input.components);
      this.setInputFields_(address);
      this.inputFieldChanged_();
      this.connectInputEvents_();
      this.languageCode = input.languageCode;
    },

    /**
     * Clears address inputs and rebuilds the input fields according to
     * |components|.
     * @private
     */
    rebuildInputFields_: function(components) {
      var content = $('autofill-edit-address-fields');
      while (content.firstChild) {
        content.removeChild(content.firstChild);
      }

      var customContainerElements = {'fullName': 'div'};
      var customInputElements = {'fullName': 'list', 'addrLines': 'textarea'};

      for (var i in components) {
        var row = document.createElement('div');
        row.classList.add('input-group', 'settings-row');
        content.appendChild(row);

        for (var j in components[i]) {
          if (components[i][j].field == 'country')
            continue;

          var fieldContainer = document.createElement(
              customContainerElements[components[i][j].field] || 'label');
          row.appendChild(fieldContainer);

          var fieldName = document.createElement('div');
          fieldName.textContent = components[i][j].name;
          fieldContainer.appendChild(fieldName);

          var input = document.createElement(
              customInputElements[components[i][j].field] || 'input');
          input.setAttribute('field', components[i][j].field);
          input.classList.add(components[i][j].length);
          input.setAttribute('placeholder', components[i][j].placeholder || '');
          fieldContainer.appendChild(input);

          if (input.tagName == 'LIST') {
            options.autofillOptions.AutofillValuesList.decorate(input);
            input.autoExpands = true;
          }
        }
      }
    },
  };

  AutofillEditAddressOverlay.loadAddress = function(address) {
    AutofillEditAddressOverlay.getInstance().loadAddress_(address);
  };

  AutofillEditAddressOverlay.loadAddressComponents = function(input) {
    AutofillEditAddressOverlay.getInstance().loadAddressComponents_(input);
  };

  AutofillEditAddressOverlay.setTitle = function(title) {
    $('autofill-address-title').textContent = title;
  };

  AutofillEditAddressOverlay.setValidatedPhoneNumbers = function(numbers) {
    var instance = AutofillEditAddressOverlay.getInstance();
    var phoneList = instance.pageDiv.querySelector('[field=phone]');
    instance.setMultiValueList_(phoneList, numbers);
    phoneList.didReceiveValidationResult();
  };

  // Export
  return {
    AutofillEditAddressOverlay: AutofillEditAddressOverlay
  };
});
