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
      $('add-user-button').addEventListener('click', function(e) {
        chrome.send('loginRequestNetworkState',
                    ['login.HeaderBar.handleAddUser',
                     'check']);
      });
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
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.disabled = value;
      }
    },

    /**
     * Cancel add user button click handler.
     * @private
     */
    handleCancelAddUserClick_: function(e) {
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

  /**
   * Continues add user button click handling after network state has
   * been recieved.
   * @param {number} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   * @param {number} last Last active network type.
   */
  HeaderBar.handleAddUser = function(state, network, reason, last) {
    if (state != NET_STATE.OFFLINE) {
      Oobe.showSigninUI();
    } else {
      /** @const */ var BUBBLE_OFFSET = 8;
      /** @const */ var BUBBLE_PADDING = 5;
      $('bubble').showTextForElement(
          $('add-user-button'),
          localStrings.getString('addUserErrorMessage'),
          cr.ui.Bubble.Attachment.TOP, BUBBLE_OFFSET, BUBBLE_PADDING);
      chrome.send('loginAddNetworkStateObserver',
                  ['login.HeaderBar.bubbleWatchdog']);
    }
  };

  /**
   * Observes network state, and close the bubble when network becomes online.
   * @param {number} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the network.
   * @param {string} reason Reason the callback was called.
   * @param {number} last Last active network type.
   */
  HeaderBar.bubbleWatchdog = function(state, network, reason, last) {
    if (state != NET_STATE.OFFLINE) {
      $('bubble').hideForElement($('add-user-button'));
      chrome.send('loginRemoveNetworkStateObserver',
                  ['login.HeaderBar.bubbleWatchdog']);
    }
  };

  return {
    HeaderBar: HeaderBar
  };
});
