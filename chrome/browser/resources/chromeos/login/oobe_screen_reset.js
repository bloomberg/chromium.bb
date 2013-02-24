// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var ResetScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  ResetScreen.register = function() {
    var screen = $('reset');
    ResetScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  ResetScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('resetScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var resetButton = this.ownerDocument.createElement('button');
      resetButton.id = 'reset-button';
      resetButton.textContent = loadTimeData.getString('resetButton');
      resetButton.addEventListener('click', function(e) {
        chrome.send('resetOnReset');
        e.stopPropagation();
      });
      buttons.push(resetButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'reset-cancel-button';
      cancelButton.textContent = loadTimeData.getString('cancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('resetOnCancel');
        e.stopPropagation();
      });
      buttons.push(cancelButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('reset-cancel-button');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('resetOnCancel');
    },
  };

  return {
    ResetScreen: ResetScreen
  };
});
