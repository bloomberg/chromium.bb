// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation.
 */

cr.define('login', function() {
  // Network state constants.
  /** @const */ var NET_STATE = {
    OFFLINE: 0,
    ONLINE: 1,
    PORTAL: 2
  };

  /**
   * Creates a header bar element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var HeaderBar = cr.ui.define('div');

  HeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether guest button should be shown when header bar is in normal mode.
    showGuest_: false,

    /** @override */
    decorate: function() {
      $('shutdown-header-bar-item').addEventListener('click',
          this.handleShutdownClick_);
      $('shutdown-button').addEventListener('click',
          this.handleShutdownClick_);
      $('add-user-button').addEventListener('click',
          this.handleAddUserClick_);
      $('cancel-add-user-button').addEventListener('click',
          this.handleCancelAddUserClick_);
      $('guest-user-header-bar-item').addEventListener('click',
          this.handleGuestClick_);
      $('guest-user-button').addEventListener('click',
          this.handleGuestClick_);
      $('sign-out-user-button').addEventListener('click',
          this.handleSignoutClick_);
    },

    /**
     * Tab index value for all button elements.
     * @type {number}
     */
    set buttonsTabIndex(tabIndex) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.tabIndex = tabIndex;
      }
    },

    /**
     * Disables the header bar and all of its elements.
     * @type {boolean}
     */
    set disabled(value) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i)
        if (!button.classList.contains('button-restricted'))
          button.disabled = value;
    },

    /**
     * Add user button click handler.
     * @private
     */
    handleAddUserClick_: function(e) {
      Oobe.showSigninUI();
      // Prevent further propagation of click event. Otherwise, the click event
      // handler of document object will set wallpaper to user's wallpaper when
      // there is only one existing user. See http://crbug.com/166477
      e.stopPropagation();
    },

    /**
     * Cancel add user button click handler.
     * @private
     */
    handleCancelAddUserClick_: function(e) {
      // Let screen handle cancel itself if that is capable of doing so.
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.cancel) {
        Oobe.getInstance().currentScreen.cancel();
        return;
      }

      $('login-header-bar').signinUIActive = false;
      $('pod-row').loadLastWallpaper();

      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
      Oobe.resetSigninUI(true);
    },

    /**
     * Guest button click handler.
     * @private
     */
    handleGuestClick_: function(e) {
      Oobe.disableSigninUI();
      chrome.send('launchIncognito');
      e.stopPropagation();
    },

    /**
     * Sign out button click handler.
     * @private
     */
    handleSignoutClick_: function(e) {
      this.disabled = true;
      chrome.send('signOutUser');
      e.stopPropagation();
    },

    /**
     * Shutdown button click handler.
     * @private
     */
    handleShutdownClick_: function(e) {
      chrome.send('shutdownSystem');
      e.stopPropagation();
    },

    /**
     * If true then "Browse as Guest" button is shown.
     * @type {boolean}
     */
    set showGuestButton(value) {
      this.showGuest_ = value;
      this.updateUI_();
    },

    /**
     * If true then sign in UI is active and header controls
     * should change accordingly.
     * @type {boolean}
     */
    set signinUIActive(value) {
      this.signinUIActive_ = value;
      this.updateUI_();
    },

    /**
     * Whether the Cancel button is enabled during Gaia sign-in.
     * @type {boolean}
     */
    set allowCancel(value) {
      this.allowCancel_ = value;
      this.updateUI_();
    },

    /**
     * Updates visibility state of action buttons.
     * @private
     */
    updateUI_: function() {
      $('add-user-header-bar-item').hidden = false;
      $('add-user-button').hidden = this.signinUIActive_;
      $('cancel-add-user-button').hidden =
          !this.signinUIActive_ || !this.allowCancel_;
      $('guest-user-header-bar-item').hidden =
          this.signinUIActive_ || !this.showGuest_;
    },

    /**
     * Animates Header bar to hide from the screen.
     * @param {function()} callback will be called once animation is finished.
     */
    animateOut: function(callback) {
      var launcher = this;
      launcher.addEventListener(
          'webkitTransitionEnd', function f(e) {
            launcher.removeEventListener('webkitTransitionEnd', f);
            callback();
          });
      this.classList.remove('login-header-bar-animate-slow');
      this.classList.add('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-hidden');
    },

    /**
     * Animates Header bar to slowly appear on the screen.
     */
    animateIn: function() {
      this.classList.remove('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-animate-slow');
      this.classList.remove('login-header-bar-hidden');
    },
  };

  return {
    HeaderBar: HeaderBar
  };
});
