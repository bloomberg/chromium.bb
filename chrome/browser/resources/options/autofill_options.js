// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  // The offset of the first profile in either the address list or the credit
  // card list. Consists of the header and the horizontal rule.
  const addressOffset = 2;
  const creditCardOffset = 3;

  /////////////////////////////////////////////////////////////////////////////
  // AutoFillOptions class:
  //
  // TODO(jhawkins): Replace <select> with a DOMUI List.

  /**
   * Encapsulated handling of AutoFill options page.
   * @constructor
   */
  function AutoFillOptions() {
    this.numAddresses = 0;
    this.numCreditCards = 0;
    this.activeNavTab = null;
    this.addressGUIDs = null;
    this.creditCardGUIDs = null;
    OptionsPage.call(this, 'autoFillOptions',
                     templateData.autoFillOptionsTitle,
                     'autoFillOptionsPage');
  }

  cr.addSingletonGetter(AutoFillOptions);

  AutoFillOptions.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('profileList').onchange = function(event) {
        self.updateButtonState_();
      };
      $('profileList').addEventListener('dblclick', function(event) {
        if ($('autoFillEnabled').checked)
          self.editProfile_();
      });
      $('addAddressButton').onclick = function(event) {
        self.showAddAddressOverlay_();
      };
      $('addCreditCardButton').onclick = function(event) {
        self.showAddCreditCardOverlay_();
      };
      $('autoFillEditButton').onclick = function(event) {
        self.editProfile_();
      };
      $('autoFillRemoveButton').onclick = function(event) {
        self.removeProfile_();
      };

      Preferences.getInstance().addEventListener('autofill.enabled',
          this.updateButtonState_.bind(this));
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
      OptionsPage.showOverlay('autoFillEditAddressOverlay');
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
      OptionsPage.showOverlay('autoFillEditAddressOverlay');
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
      OptionsPage.showOverlay('autoFillEditCreditCardOverlay');
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
      OptionsPage.showOverlay('autoFillEditCreditCardOverlay');
    },

    /**
     * Resets the address list. This method leaves the header and horizontal
     * rule unchanged.
     * @private
     */
    resetAddresses_: function() {
      var profiles = $('profileList');
      for (var i = 0; i <  this.numAddresses; ++i)
        profiles.remove(addressOffset);
      this.numAddresses = 0;
    },

    /**
     * Resets the credit card list. This method leaves the header and horizontal
     * rule unchanged.
     * @private
     */
    resetCreditCards_: function() {
      var profiles = $('profileList');
      var offset = this.numAddresses + addressOffset + creditCardOffset;
      for (var i = 0; i <  this.numCreditCards; ++i)
        profiles.remove(offset);
      this.numCreditCards = 0;
    },

    /**
     * Updates the address list with the given entries.
     * @private
     * @param {Array} address List of addresses.
     */
    updateAddresses_: function(addresses) {
      this.resetAddresses_();
      var profileList = $('profileList');
      var blankAddress = profileList.options[addressOffset];
      this.numAddresses = addresses.length;
      this.addressGUIDs = new Array(this.numAddresses);
      for (var i = 0; i < this.numAddresses; i++) {
        var address = addresses[i];
        var option = new Option(address['label']);
        this.addressGUIDs[i] = address['guid'];
        profileList.add(option, blankAddress);
      }

      this.updateButtonState_();
    },

    /**
     * Updates the credit card list with the given entries.
     * @private
     * @param {Array} creditCards List of credit cards.
     */
    updateCreditCards_: function(creditCards) {
      this.resetCreditCards_();
      var profileList = $('profileList');
      this.numCreditCards = creditCards.length;
      this.creditCardGUIDs = new Array(this.numCreditCards);
      for (var i = 0; i < this.numCreditCards; i++) {
        var creditCard = creditCards[i];
        var option = new Option(creditCard['label']);
        this.creditCardGUIDs[i] = creditCard['guid'];
        profileList.add(option, null);
      }

      this.updateButtonState_();
    },

    /**
     * Sets the enabled state of the AutoFill Add Address and Credit Card
     * buttons on the current state of the |autoFillEnabled| checkbox.
     * Sets the enabled state of the AutoFill Edit and Remove buttons based on
     * the current selection in the profile list.
     * @private
     */
    updateButtonState_: function() {
      var disabled = !$('autoFillEnabled').checked;
      $('addAddressButton').disabled = disabled;
      $('addCreditCardButton').disabled = disabled;
      $('autoFillRemoveButton').disabled = $('autoFillEditButton').disabled =
          disabled || ($('profileList').selectedIndex == -1);
    },

    /**
     * Calls into the browser to load either an address or a credit card,
     * depending on the selected index.  The browser calls back into either
     * editAddress() or editCreditCard() which show their respective editors.
     * @private
     */
    editProfile_: function() {
      var idx = $('profileList').selectedIndex;
      if ((profileIndex = this.getAddressIndex_(idx)) != -1) {
        chrome.send('editAddress', [this.addressGUIDs[profileIndex]]);
      } else if ((profileIndex = this.getCreditCardIndex_(idx)) != -1) {
        chrome.send('editCreditCard', [this.creditCardGUIDs[profileIndex]]);
      }
    },

    /**
     * Removes the currently selected profile, whether it's an address or a
     * credit card.
     * @private
     */
    removeProfile_: function() {
      var idx = $('profileList').selectedIndex;
      if ((profileIndex = this.getAddressIndex_(idx)) != -1)
        chrome.send('removeAddress', [this.addressGUIDs[profileIndex]]);
      else if ((profileIndex = this.getCreditCardIndex_(idx)) != -1)
        chrome.send('removeCreditCard', [this.creditCardGUIDs[profileIndex]]);
    },

    /**
     * Returns the index into the address list based on |index|, the index into
     * the select control. Returns -1 if this is not an address index.
     * @private
     */
    getAddressIndex_: function(index) {
      index -= addressOffset;
      if (index >= 0 && index < this.numAddresses)
        return index;

      return -1;
    },

    /**
     * Returns the index into the credit card list based on |index|, the index
     * into the select control. Returns -1 if this is not a credit card index.
     * @private
     */
    getCreditCardIndex_: function(index) {
      index -= addressOffset + this.numAddresses + creditCardOffset;
      if (index >= 0 && index < this.numCreditCards)
        return index;

      return -1;
    },

    /**
     * Returns true if |index| points to a credit card profile.
     * @private
     */
    profileIndexIsCreditCard_: function(index) {
      index -= addressOffset + this.numAddresses + creditCardOffset;
      return (index >= 0 && index < this.numCreditCards);
    }
  };

  AutoFillOptions.updateAddresses = function(addresses) {
    AutoFillOptions.getInstance().updateAddresses_(addresses);
  };

  AutoFillOptions.editAddress = function(address) {
    AutoFillOptions.getInstance().showEditAddressOverlay_(address);
  };

  AutoFillOptions.updateCreditCards = function(creditCards) {
    AutoFillOptions.getInstance().updateCreditCards_(creditCards);
  };

  AutoFillOptions.editCreditCard = function(creditCard) {
    AutoFillOptions.getInstance().showEditCreditCardOverlay_(creditCard);
  };

  // Export
  return {
    AutoFillOptions: AutoFillOptions
  };

});

