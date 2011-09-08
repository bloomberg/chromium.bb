// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe update screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UpdateScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  UpdateScreen.register = function() {
    var screen = $('update');
    UpdateScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  UpdateScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('updateScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      return null;
    },

    /**
     * Cancels the screen.
     */
    cancel: function() {
      // It's safe to act on the accelerator even if it's disabled on official
      // builds, since Chrome will just ignore the message in that case.
      var updateCancelHint = $('update-cancel-hint').firstElementChild;
      updateCancelHint.textContent =
          localStrings.getString('cancelledUpdateMessage');
      chrome.send('cancelUpdate');
    }
  };

  /**
   * Makes 'press Escape to cancel update' hint visible.
   */
  UpdateScreen.enableUpdateCancel = function() {
    $('update-cancel-hint').hidden = false;
  };

  return {
    UpdateScreen: UpdateScreen
  };
});
