// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Account picker screen implementation.
 */

cr.define('login', function() {
  /**
   * Maximum number of offline login failures before online login.
   * @type {number}
   * @const
   */
  var MAX_LOGIN_ATTEMPTS_IN_POD = 3;
  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

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

    /** @override */
    decorate: function() {
      login.PodRow.decorate($('pod-row'));
    },

    // Whether this screen is shown for the first time.
    firstShown_: true,

    /**
     * When the account picker is being used to lock the screen, pressing the
     * exit accelerator key will sign out the active user as it would when
     * they are signed in.
     */
    exit: function() {
      // Check and disable the sign out button so that we can never have two
      // sign out requests generated in a row.
      if ($('pod-row').lockedPod && !$('sign-out-user-button').disabled) {
        $('sign-out-user-button').disabled = true;
        chrome.send('signOutUser');
      }
    },

    /**
     * Event handler that is invoked just after the frame is shown.
     * @param {string} data Screen init payload.
     */
    onAfterShow: function(data) {
      $('pod-row').handleAfterShow();
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      chrome.send('loginUIStateChanged', ['account-picker', true]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
      chrome.send('hideCaptivePortal');
      var podRow = $('pod-row');
      podRow.handleBeforeShow();

      // If this is showing for the lock screen display the sign out button,
      // hide the add user button and activate the locked user's pod.
      var lockedPod = podRow.lockedPod;
      $('add-user-header-bar-item').hidden = !!lockedPod;
      $('sign-out-user-item').hidden = !lockedPod;
      // In case of the preselected pod onShow will be called once pod
      // receives focus.
      if (!podRow.preselectedPod)
        this.onShow();
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onShow: function() {
      if (!this.firstShown_) return;
      this.firstShown_ = false;
      // TODO(nkostylev): Enable animation back when session start jank
      // is reduced. See http://crosbug.com/11116 http://crosbug.com/18307
      // $('pod-row').startInitAnimation();

      chrome.send('accountPickerReady');
      chrome.send('loginVisible', ['account-picker']);
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
      chrome.send('loginUIStateChanged', ['account-picker', false]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
      $('pod-row').handleHide();
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} content Content to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      var activatedPod = $('pod-row').activatedPod;
      if (!activatedPod) {
        $('bubble').showContentForElement($('pod-row'),
                                          cr.ui.Bubble.Attachment.RIGHT,
                                          error);
        return;
      }
      // Show web authentication if this is not a locally managed user.
      if (loginAttempts > MAX_LOGIN_ATTEMPTS_IN_POD &&
          !activatedPod.user.locallyManagedUser) {
        activatedPod.showSigninUI();
      } else {
        // We want bubble's arrow to point to the first letter of input.
        /** @const */ var BUBBLE_OFFSET = 7;
        /** @const */ var BUBBLE_PADDING = 4;
        $('bubble').showContentForElement(activatedPod.mainInput,
                                          cr.ui.Bubble.Attachment.BOTTOM,
                                          error,
                                          BUBBLE_OFFSET, BUBBLE_PADDING);
      }
    }
  };

  /**
   * Loads givens users in pod row.
   * @param {array} users Array of user.
   * @param {boolean} animated Whether to use init animation.
   * @param {boolean} showGuest Whether to show guest session button.
   */
  AccountPickerScreen.loadUsers = function(users, animated, showGuest) {
    $('pod-row').loadPods(users, animated);
    $('login-header-bar').showGuestButton = showGuest;
  };

  /**
   * Updates current image of a user.
   * @param {string} username User for which to update the image.
   */
  AccountPickerScreen.updateUserImage = function(username) {
    $('pod-row').updateUserImage(username);
  };

  /**
   * Updates user to use gaia login.
   * @param {string} username User for which to state the state.
   */
  AccountPickerScreen.updateUserGaiaNeeded = function(username) {
    $('pod-row').resetUserOAuthTokenStatus(username);
  };

  /**
   * Updates Caps Lock state (for Caps Lock hint in password input field).
   * @param {boolean} enabled Whether Caps Lock is on.
   */
  AccountPickerScreen.setCapsLockState = function(enabled) {
    $('pod-row').classList[enabled ? 'add' : 'remove']('capslock-on');
  };

  /**
   * Enforces focus on user pod of locked user.
   */
  AccountPickerScreen.forceLockedUserPodFocus = function() {
    var row = $('pod-row');
    if (row.lockedPod)
      row.focusPod(row.lockedPod, true);
  };

  return {
    AccountPickerScreen: AccountPickerScreen
  };
});
