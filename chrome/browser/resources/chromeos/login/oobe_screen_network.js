// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe network screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var NetworkScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  NetworkScreen.register = function() {
    var screen = $('connect');
    NetworkScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  NetworkScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Dropdown element for networks selection.
     */
    dropdown_: null,

    /** @inheritDoc */
    decorate: function() {
      Oobe.setupSelect($('language-select'),
                       templateData.languageList,
                       'networkOnLanguageChanged');

      Oobe.setupSelect($('keyboard-select'),
                       templateData.inputMethodsList,
                       'networkOnInputMethodChanged');

      this.dropdown_ = $('networks-list');
      cr.ui.DropDown.decorate(this.dropdown_);
    },

    onBeforeShow: function(data) {
      cr.ui.DropDown.setActive('networks-list', true, true);
    },

    onBeforeHide: function() {
      cr.ui.DropDown.setActive('networks-list', false, true);
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('networkScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'continue-button';
      continueButton.textContent = localStrings.getString('continueButton');
      continueButton.addEventListener('click', function(e) {
        chrome.send('networkOnExit', []);
      });
      buttons.push(continueButton);

      return buttons;
    }
  };

  /**
   * Shows the network error message.
   * @param {string} message Message to be shown.
   */
  NetworkScreen.showError = function(message) {
    var error = document.createElement('div');
    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    $('bubble').showContentForElement($('networks-list'), error);
  };

  /**
   * Hides the error notification bubble (if any).
   */
  NetworkScreen.clearErrors = function() {
    $('bubble').hide();
  };

  return {
    NetworkScreen: NetworkScreen
  };
});
