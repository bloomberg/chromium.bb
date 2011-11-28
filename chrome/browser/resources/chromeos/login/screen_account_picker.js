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
      var podRow = $('pod-row');
      podRow.handleShow();

      // If this is showing for the lock screen display the sign out button,
      // hide the add user button and activate the locked user's pod.
      var lockedPod = podRow.lockedPod;
      $('add-user-header-bar-item').hidden = !!lockedPod;
      $('sign-out-user-item').hidden = !lockedPod;
      if (lockedPod)
        podRow.focusPod(lockedPod);

      if (this.firstShown_) {
        this.firstShown_ = false;
        // TODO(nkostylev): Enable animation back when session start jank
        // is reduced. See http://crosbug.com/11116 http://crosbug.com/18307
        // $('pod-row').startInitAnimation();

        // TODO(altimofeev): Call it after animation has stoped when animation
        // is enabled.
        chrome.send('accountPickerReady', []);
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

  /**
   * Updates current image of a user.
   * @param {string} email Email of the user for which to update the image.
   * @public
   */
  AccountPickerScreen.updateUserImage = function(email) {
    $('pod-row').updateUserImage(email);
  };

  /**
   * Updates Caps Lock state (for Caps Lock hint in password input field).
   * @param {boolean} enabled Whether Caps Lock is on.
   * @public
   */
  AccountPickerScreen.setCapsLockState = function(enabled) {
    $('pod-row').classList[enabled ? 'add' : 'remove']('capslock-on');
  };

  return {
    AccountPickerScreen: AccountPickerScreen
  };
});
