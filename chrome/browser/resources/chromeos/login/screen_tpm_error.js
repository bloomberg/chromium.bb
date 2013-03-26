// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  /**
   * Creates a new TPM error message screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TPMErrorMessageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  TPMErrorMessageScreen.register = function() {
    var screen = $('tpm-error-message');
    TPMErrorMessageScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  TPMErrorMessageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
     get buttons() {
       var rebootButton = this.ownerDocument.createElement('button');
       rebootButton.id = 'reboot-button';
       rebootButton.textContent =
         loadTimeData.getString('errorTpmFailureRebootButton');
       rebootButton.addEventListener('click', function() {
           chrome.send('rebootSystem');
       });
       return [rebootButton];
     },
  };

  /**
   * Show TPM screen.
   */
  TPMErrorMessageScreen.show = function() {
    Oobe.showScreen({id: SCREEN_TPM_ERROR});
  };

  return {
    TPMErrorMessageScreen: TPMErrorMessageScreen
  };
});
