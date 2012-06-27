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
  var BACKGROUND_TRANSITION_DURATION_MS = 1000;
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

    /** @inheritDoc */
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
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      chrome.send('hideCaptivePortal');
      var podRow = $('pod-row');
      podRow.handleShow();

      // If this is showing for the lock screen display the sign out button,
      // hide the add user button and activate the locked user's pod.
      var lockedPod = podRow.lockedPod;
      $('add-user-header-bar-item').hidden = !!lockedPod;
      $('sign-out-user-item').hidden = !lockedPod;
      var preselectedPod = PRESELECT_FIRST_POD ?
          lockedPod || podRow.pods[0] : lockedPod;
      if (preselectedPod) {
        // TODO(altimofeev): empirically I investigated that focus isn't
        // set correctly if following CSS rules are present:
        //
        // podrow {
        //   -webkit-transition: all 200ms ease-in-out;
        // }
        // .pod {
        //  -webkit-transition: all 230ms ease;
        // }
        //
        // Workaround is either delete these rules or delay the focus setting.
        var self = this;
        preselectedPod.addEventListener('webkitTransitionEnd', function f(e) {
          if (e.target == preselectedPod) {
            podRow.focusPod(preselectedPod);
            preselectedPod.removeEventListener(f);
            // Delay the accountPickerReady signal so that if there are any
            // timeouts waiting to fire we can process these first. This was
            // causing crbug.com/112218 as the account pod was sometimes focuse
            // using focusPod (which resets the password) after the test code
            // set the password.
            self.onShow();
          }
        });
      } else {
        this.onShow();
      }
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

      // TODO(altimofeev): Call it after animation has stoped when animation
      // is enabled.
      chrome.send('accountPickerReady');
    },

     /**
      * Event handler that is invoked just before the frame is hidden.
      * @param {string} data Screen init payload.
      */
    onBeforeHide: function(data) {
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
        $('bubble').showContentForElement($('pod-row'), error,
                                          cr.ui.Bubble.Attachment.RIGHT);
        return;
      }
      if (loginAttempts > MAX_LOGIN_ATTEMPTS_IN_POD) {
        activatedPod.showSigninUI();
      } else {
        $('bubble').showContentForElement(activatedPod.mainInput, error,
                                          cr.ui.Bubble.Attachment.BOTTOM);
      }
    }
  };

  /**
   * Loads givens users in pod row.
   * @param {array} users Array of user.
   * @param {boolean} animated Whether to use init animation.
   */
  AccountPickerScreen.loadUsers = function(users, animated) {
    $('pod-row').loadPods(users, animated);
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
   * Sets wallpaper for lock screen.
   */
  AccountPickerScreen.setWallpaper = function() {
    var oobe = Oobe.getInstance();
    if (!oobe.isNewOobe() || !oobe.isLockScreen())
      return;
    // Prepare animation.
    var from = $('background');
    var to = $('background-transition');
    var start;
    var duration = BACKGROUND_TRANSITION_DURATION_MS;
    function radialfade(timestamp) {
      var progress = 100 * (timestamp - start) / duration;
      to.style.webkitMaskBoxImage =
          '-webkit-radial-gradient( 50% 50%, circle cover, rgba(0,0,0,1) ' +
              progress + '%, rgba(0,0,0,0) ' + (progress + 60) + '%)';
      if (progress < 100) {
        webkitRequestAnimationFrame(radialfade);
      } else {
        from.style.backgroundImage = to.style.backgroundImage;
        to.style.webkitMaskBoxImage = '';
        to.style.backgroundImage = '';
      }
    }
    // Load image before starting animation.
    var image = new Image();
    image.onload = function() {
      to.style.backgroundImage = 'url(' + image.src + ')';
      start = new Date().getTime();
      webkitRequestAnimationFrame(radialfade);
    };
    // Start image loading.
    // Add timestamp for wallpapers that are rotated over time.
    image.src = 'chrome://wallpaper/' + new Date().getTime();
  };

  return {
    AccountPickerScreen: AccountPickerScreen
  };
});
