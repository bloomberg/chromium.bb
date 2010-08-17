// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  // The offset of the first profile in either the address list or the credit
  // card list. Consists of the header and the horizontal rule.
  const profileOffset = 2;

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
        self.updateRemoveButtonState_();
      };
    },

    /**
     * Resets the address list. This method leaves the header and horizontal
     * rule unchanged.
     * @private
     */
    resetAddresses_: function() {
      var profiles = $('profileList');
      for (var i = 0; i <  this.numAddresses; ++i)
        profiles.remove(profileOffset);
    },

    /**
     * Resets the credit card list. This method leaves the header and horizontal
     * rule unchanged.
     * @private
     */
    resetCreditCards_: function() {
      var profiles = $('profileList');
      var offset = this.numAddresses + profileOffset;
      for (var i = 0; i <  this.numCreditCards; ++i)
        profiles.remove(offset);
    },

    /**
     * Updates the address list with the given entries.
     * @private
     * @param {Array} address List of addresses.
     */
    updateAddresses_: function(addresses) {
      this.resetAddresses_();
      profileList = $('profileList');
      var blankAddress =
          profileList.options[profileOffset + this.numAddresses];
      this.numAddresses = addresses.length;
      for (var i = 0; i < this.numAddresses; i++) {
        var address = addresses[i];
        var option = new Option(address['label']);
        profileList.add(option, blankAddress);
      }

      this.updateRemoveButtonState_();
    },

    /**
     * Updates the credit card list with the given entries.
     * @private
     * @param {Array} creditCards List of credit cards.
     */
    updateCreditCards_: function(creditCards) {
      this.resetCreditCards_();
      profileList = $('profileList');
      this.numCreditCards = creditCards.length;
      for (var i = 0; i < this.numCreditCards; i++) {
        var creditCard = creditCards[i];
        var option = new Option(creditCard['label']);
        profileList.add(option, null);
      }

      this.updateRemoveButtonState_();
    },

    /**
     * Sets the enabled state of the AutoFill Remove button based on the current
     * selection in the profile list.
     * @private
     */
    updateRemoveButtonState_: function() {
      $('autoFillRemoveButton').disabled =
          ($('profileList').selectedIndex == -1);
    },
  };

  AutoFillOptions.updateAddresses = function(addresses) {
    AutoFillOptions.getInstance().updateAddresses_(addresses);
  };

  AutoFillOptions.updateCreditCards = function(creditCards) {
    AutoFillOptions.getInstance().updateCreditCards_(creditCards);
  };

  // Export
  return {
    AutoFillOptions: AutoFillOptions
  };

});

