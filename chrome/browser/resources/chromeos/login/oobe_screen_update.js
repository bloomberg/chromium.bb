// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  /** @const */ var ELLIPSIS = ['', '.', '..', '...'];
  /** @const */ var ELLIPSIS_ANIMATION_TIMEOUT_MS = 1000;

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

    /** @override */
    decorate: function() {
    },

    onBeforeShow: function(data) {
      UpdateScreen.startEllipsisAnimation();
    },

    onBeforeHide: function() {
      UpdateScreen.stopEllipsisAnimation();
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('updateScreenTitle');
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
          loadTimeData.getString('cancelledUpdateMessage');
      chrome.send('cancelUpdate');
    },
  };

  var ellipsis_animation_active = false;

  /**
   * Updates number of dots in the ellipsis.
   *
   * @private
   * @param {number} count Number of dots that should be shown.
   */
  function updateEllipsisAnimation_(count) {
    $('update-checking-ellipsis').textContent = ELLIPSIS[count];
    if (ellipsis_animation_active) {
      window.setTimeout(function() {
          updateEllipsisAnimation_((count + 1) % ELLIPSIS.length);
        }, ELLIPSIS_ANIMATION_TIMEOUT_MS);
    }
  };

  /**
   * Makes 'press Escape to cancel update' hint visible.
   */
  UpdateScreen.enableUpdateCancel = function() {
    $('update-cancel-hint').hidden = false;
  };

  /**
   * Starts animation for tail ellipses in "Checking for update..." label.
   */
  UpdateScreen.startEllipsisAnimation = function() {
    ellipsis_animation_active = true;
    updateEllipsisAnimation_(0);
  };

  /**
   * Stops animation for tail ellipses in "Checking for update..." label.
   */
  UpdateScreen.stopEllipsisAnimation = function() {
    ellipsis_animation_active = false;
  };

  return {
    UpdateScreen: UpdateScreen
  };
});
