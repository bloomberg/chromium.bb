// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Account picker screen implementation.
 */

cr.define('login', function() {
  /**
   * Creates a new account picker screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var AccountPickerScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  AccountPickerScreen.register = function() {
    var screen = $('account-picker');
    AccountPickerScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  AccountPickerScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      login.PodRow.decorate($('pod-row'));
    },

    // Whether this screen is shown for the first time.
    firstShown_ : true,

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param data {string} Screen init payload.
     */
    onBeforeShow: function(data) {
      $('pod-row').handleShow();
      if (this.firstShown_) {
        this.firstShown_ = false;
        $('pod-row').startInitAnimation();
      }
    },

     /**
      * Event handler that is invoked just before the frame is hidden.
      * @param data {string} Screen init payload.
      */
    onBeforeHide: function(data) {
      $('pod-row').handleHide();
    }
  };

  /**
   * Loads givens users in pod row.
   * @param {array} users Array of user.
   * @param {boolean} animated Whether to use init animation.
   * @public
   */
  AccountPickerScreen.loadUsers = function(users, animated) {
    $('pod-row').loadPods(users, animated);
  };

  return {
    AccountPickerScreen: AccountPickerScreen
  };
});
