// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe network screen implementation.
 */

login.createScreen('NetworkScreen', 'connect', function() {
  return {
    EXTERNAL_API: [
      'showError'
    ],

    /**
     * Dropdown element for networks selection.
     */
    dropdown_: null,

    /** @override */
    decorate: function() {
      Oobe.setupSelect($('language-select'),
                       loadTimeData.getValue('languageList'),
                       'networkOnLanguageChanged');

      Oobe.setupSelect($('keyboard-select'),
                       loadTimeData.getValue('inputMethodsList'),
                       'networkOnInputMethodChanged');

      this.dropdown_ = $('networks-list');
      cr.ui.DropDown.decorate(this.dropdown_);
    },

    onBeforeShow: function(data) {
      cr.ui.DropDown.show('networks-list', true, -1);
    },

    onBeforeHide: function() {
      cr.ui.DropDown.hide('networks-list');
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('networkScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'continue-button';
      continueButton.textContent = loadTimeData.getString('continueButton');
      continueButton.addEventListener('click', function(e) {
        chrome.send('networkOnExit');
        e.stopPropagation();
      });
      buttons.push(continueButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('language-select');
    },

    /**
     * Shows the network error message.
     * @param {string} message Message to be shown.
     */
    showError: function(message) {
      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent = message;
      error.appendChild(messageDiv);

      $('bubble').showContentForElement($('networks-list'),
                                        cr.ui.Bubble.Attachment.BOTTOM,
                                        error);
    }
  };
});

