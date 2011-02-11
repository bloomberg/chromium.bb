// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // AutoFillOptions class:

  /**
   * Encapsulated handling of AutoFill options page.
   * @constructor
   */
  function AutoFillOptions() {
    OptionsPage.call(this,
                     'autoFillOptions',
                     templateData.autoFillOptionsPageTabTitle,
                     'autofill-options');
  }

  cr.addSingletonGetter(AutoFillOptions);

  AutoFillOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The address list.
     * @type {DeletableItemList}
     * @private
     */
    addressList_: null,

    /**
     * The credit card list.
     * @type {DeletableItemList}
     * @private
     */
    creditCardList_: null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createAddressList_();
      this.createCreditCardList_();

      var self = this;
      $('autofill-add-address').onclick = function(event) {
        self.showAddAddressOverlay_();
      };
      $('autofill-add-creditcard').onclick = function(event) {
        self.showAddCreditCardOverlay_();
      };

      // TODO(jhawkins): What happens when AutoFill is disabled whilst on the
      // AutoFill options page?
    },

    /**
     * Creates, decorates and initializes the address list.
     * @private
     */
    createAddressList_: function() {
      this.addressList_ = $('address-list');
      options.autoFillOptions.AutoFillAddressList.decorate(this.addressList_);
      this.addressList_.autoExpands = true;
    },

    /**
     * Creates, decorates and initializes the credit card list.
     * @private
     */
    createCreditCardList_: function() {
      this.creditCardList_ = $('creditcard-list');
      options.autoFillOptions.AutoFillCreditCardList.decorate(
          this.creditCardList_);
      this.creditCardList_.autoExpands = true;
    },

    /**
     * Shows the 'Add address' overlay, specifically by loading the
     * 'Edit address' overlay, emptying the input fields and modifying the
     * overlay title.
     * @private
     */
    showAddAddressOverlay_: function() {
      var title = localStrings.getString('addAddressTitle');
      AutoFillEditAddressOverlay.setTitle(title);
      AutoFillEditAddressOverlay.clearInputFields();
      OptionsPage.navigateToPage('autoFillEditAddressOverlay');
    },

    /**
     * Shows the 'Add credit card' overlay, specifically by loading the
     * 'Edit credit card' overlay, emptying the input fields and modifying the
     * overlay title.
     * @private
     */
    showAddCreditCardOverlay_: function() {
      var title = localStrings.getString('addCreditCardTitle');
      AutoFillEditCreditCardOverlay.setTitle(title);
      AutoFillEditCreditCardOverlay.clearInputFields();
      OptionsPage.navigateToPage('autoFillEditCreditCardOverlay');
    },

    /**
     * Updates the data model for the address list with the values from
     * |entries|.
     * @param {Array} entries The list of addresses.
     */
    setAddressList_: function(entries) {
      this.addressList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Updates the data model for the credit card list with the values from
     * |entries|.
     * @param {Array} entries The list of credit cards.
     */
    setCreditCardList_: function(entries) {
      this.creditCardList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Removes the AutoFill address represented by |guid|.
     * @param {String} guid The GUID of the address to remove.
     * @private
     */
    removeAddress_: function(guid) {
      chrome.send('removeAddress', [guid]);
    },

    /**
     * Removes the AutoFill credit card represented by |guid|.
     * @param {String} guid The GUID of the credit card to remove.
     * @private
     */
    removeCreditCard_: function(guid) {
      chrome.send('removeCreditCard', [guid]);
    },

    /**
     * Requests profile data for the address represented by |guid| from the
     * PersonalDataManager. Once the data is loaded, the AutoFillOptionsHandler
     * calls showEditAddressOverlay().
     * @param {String} guid The GUID of the address to edit.
     * @private
     */
    loadAddressEditor_: function(guid) {
      chrome.send('loadAddressEditor', [guid]);
    },

    /**
     * Requests profile data for the credit card represented by |guid| from the
     * PersonalDataManager. Once the data is loaded, the AutoFillOptionsHandler
     * calls showEditCreditCardOverlay().
     * @param {String} guid The GUID of the credit card to edit.
     * @private
     */
    loadCreditCardEditor_: function(guid) {
      chrome.send('loadCreditCardEditor', [guid]);
    },

    /**
     * Shows the 'Edit address' overlay, using the data in |address| to fill the
     * input fields. |address| is a list with one item, an associative array
     * that contains the address data.
     * @private
     */
    showEditAddressOverlay_: function(address) {
      var title = localStrings.getString('editAddressTitle');
      AutoFillEditAddressOverlay.setTitle(title);
      AutoFillEditAddressOverlay.loadAddress(address[0]);
      OptionsPage.navigateToPage('autoFillEditAddressOverlay');
    },

    /**
     * Shows the 'Edit credit card' overlay, using the data in |credit_card| to
     * fill the input fields. |address| is a list with one item, an associative
     * array that contains the credit card data.
     * @private
     */
    showEditCreditCardOverlay_: function(creditCard) {
      var title = localStrings.getString('editCreditCardTitle');
      AutoFillEditCreditCardOverlay.setTitle(title);
      AutoFillEditCreditCardOverlay.loadCreditCard(creditCard[0]);
      OptionsPage.navigateToPage('autoFillEditCreditCardOverlay');
    },
  };

  AutoFillOptions.setAddressList = function(entries) {
    AutoFillOptions.getInstance().setAddressList_(entries);
  };

  AutoFillOptions.setCreditCardList = function(entries) {
    AutoFillOptions.getInstance().setCreditCardList_(entries);
  };

  AutoFillOptions.removeAddress = function(guid) {
    AutoFillOptions.getInstance().removeAddress_(guid);
  };

  AutoFillOptions.removeCreditCard = function(guid) {
    AutoFillOptions.getInstance().removeCreditCard_(guid);
  };

  AutoFillOptions.loadAddressEditor = function(guid) {
    AutoFillOptions.getInstance().loadAddressEditor_(guid);
  };

  AutoFillOptions.loadCreditCardEditor = function(guid) {
    AutoFillOptions.getInstance().loadCreditCardEditor_(guid);
  };

  AutoFillOptions.editAddress = function(address) {
    AutoFillOptions.getInstance().showEditAddressOverlay_(address);
  };

  AutoFillOptions.editCreditCard = function(creditCard) {
    AutoFillOptions.getInstance().showEditCreditCardOverlay_(creditCard);
  };

  // Export
  return {
    AutoFillOptions: AutoFillOptions
  };

});

