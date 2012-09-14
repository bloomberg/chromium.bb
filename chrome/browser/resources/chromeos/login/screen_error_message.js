// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  // Screens that should have offline message overlay.
  /** @const */ var MANAGED_SCREENS = ['gaia-signin'];

  // Network state constants.
  var NET_STATE = {
    OFFLINE: 0,
    ONLINE: 1,
    PORTAL: 2,
    CONNECTING: 3
  };

  // Error reasons which are passed to updateState_() method.
  /** @const */ var ERROR_REASONS = {
    PROXY_AUTH_CANCELLED: 'frame error:111',
    PROXY_CONNECTION_FAILED: 'frame error:130',
    PROXY_CONFIG_CHANGED: 'proxy changed',
    LOADING_TIMEOUT: 'loading timeout',
    PORTAL_DETECTED: 'portal detected',
    NETWORK_CHANGED: 'network changed'
  };

  // Frame loading errors.
  var NET_ERROR = {
    ABORTED_BY_USER: 3
  };

  // Link which starts guest session for captive portal fixing.
  /** @const */ var FIX_CAPTIVE_PORTAL_ID = 'captive-portal-fix-link';

  /** @const */ var FIX_PROXY_SETTINGS_ID = 'proxy-settings-fix-link';

  // Id of the element which holds current network name.
  /** @const */ var CURRENT_NETWORK_NAME_ID = 'captive-portal-network-name';

  // Link which triggers frame reload.
  /** @const */ var RELOAD_PAGE_ID = 'proxy-error-retry-link';

  // Timeout used to delay first offline notification from network
  // manager.
  /** @const */ var OFFLINE_TIMEOUT_SEC = 5;

  // Timeout used to prevent infinite connecting to a flaky network.
  /** @const */ var CONNECTING_TIMEOUT_SEC = 60;

  /**
   * Creates a new offline message screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var ErrorMessageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  ErrorMessageScreen.register = function() {
    var screen = $('error-message');
    ErrorMessageScreen.decorate(screen);

    // Note that ErrorMessageScreen is not registered with Oobe because
    // it is shown on top of sign-in screen instead of as an independent screen.
  };

  ErrorMessageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      chrome.send('loginAddNetworkStateObserver',
                  ['login.ErrorMessageScreen.updateState']);

      cr.ui.DropDown.decorate($('offline-networks-list'));
      this.updateLocalizedContent_();
    },

    /**
     * Timer used to delay calls to updateState_ method.
     */
    updateStateTimer_: undefined,

    /**
     * True if updateState_ method was not called.
     */
    firstUpdateStateCall_: true,

    /*
     * Timer which is started when network moves to the connecting
     * state. If it's triggered, then it's look like the network is
     * flaky and we must show offline message.
     */
    connectingTimer_: undefined,

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent_: function() {
      $('captive-portal-message-text').innerHTML = localStrings.getStringF(
        'captivePortalMessage',
        '<b id="' + CURRENT_NETWORK_NAME_ID + '"></b>',
        '<a id="' + FIX_CAPTIVE_PORTAL_ID + '" class="signin-link" href="#">',
        '</a>');
      $(FIX_CAPTIVE_PORTAL_ID).onclick = function() {
        chrome.send('showCaptivePortal');
      };

      $('captive-portal-proxy-message-text').innerHTML =
        localStrings.getStringF(
          'captivePortalProxyMessage',
          '<a id="' + FIX_PROXY_SETTINGS_ID + '" class="signin-link" href="#">',
          '</a>');
      $(FIX_PROXY_SETTINGS_ID).onclick = function() {
        chrome.send('openProxySettings');
      };

      $('proxy-message-text').innerHTML = localStrings.getStringF(
          'proxyMessageText',
          '<a id="' + RELOAD_PAGE_ID + '" class="signin-link" href="#">',
          '</a>');
      $(RELOAD_PAGE_ID).onclick = function() {
        var currentScreen = Oobe.getInstance().currentScreen;
        // Schedules a immediate retry.
        currentScreen.doReload();
      };

      $('error-guest-signin').innerHTML = localStrings.getStringF(
          'guestSignin',
          '<a id="error-guest-signin-link" class="signin-link" href="#">',
          '</a>');
      $('error-guest-signin-link').onclick = function() {
        chrome.send('launchIncognito');
      };

      $('error-offline-login').innerHTML = localStrings.getStringF(
          'offlineLogin',
          '<a id="error-offline-login-link" class="signin-link" href="#">',
          '</a>');
      $('error-offline-login-link').onclick = function() {
        chrome.send('offlineLogin');
      };
    },

    onBeforeShow: function(lastNetworkType) {
      var currentScreen = Oobe.getInstance().currentScreen;

      cr.ui.DropDown.show('offline-networks-list', false, lastNetworkType);

      $('error-guest-signin').hidden = $('guestSignin').hidden ||
          !$('add-user-header-bar-item').hidden;

      $('error-offline-login').hidden = !currentScreen.isOfflineAllowed;
    },

    onBeforeHide: function() {
      cr.ui.DropDown.hide('offline-networks-list');
    },

    update: function() {
      chrome.send('loginRequestNetworkState',
                  ['login.ErrorMessageScreen.updateState',
                   'update']);
    },

    /**
     * Clears |updateStateTimer_|.
     * @private
     */
    clearUpdateStateTimer_: function() {
      if (this.updateStateTimer_) {
        window.clearTimeout(this.updateStateTimer_);
        this.updateStateTimer_ = undefined;
      }
    },

    /**
     * Clears |connectingTimer_|.
     * @private
     */
    clearConnectingTimer_: function() {
      if (this.connectingTimer_) {
        window.clearTimeout(this.connectingTimer_);
        this.connectingTimer_ = undefined;
      }
    },

    /**
     * Shows or hides offline message based on network on/offline state.
     * @param {number} state Current state of the network (see NET_STATE).
     * @param {string} network Name of the current network.
     * @param {string} reason Reason the callback was called.
     * @param {number} lastNetworkType Last active network type.
     * @param {boolean} opt_forceUpdate Are state should be updated in any case?
     */
    updateState_: function(state, network, reason, lastNetworkType,
                           opt_forceUpdate) {
      console.log('updateState_: state=' + state +
                  ', network=' + network + ', reason=' + reason +
                  ', lastNetworkType=' + lastNetworkType +
                  ', opt_forceUpdate=' + opt_forceUpdate);

      this.clearUpdateStateTimer_();

      // Delay first notification about offline state.
      if (state == NET_STATE.OFFLINE && this.firstUpdateStateCall_) {
        this.firstUpdateStateCall_ = false;
        this.updateStateTimer_ = window.setTimeout(
            this.updateState_.bind(
                this, state, network, reason, lastNetworkType, opt_forceUpdate),
            OFFLINE_TIMEOUT_SEC * 1000);
        return;
      }
      this.firstUpdateStateCall_ = false;

      // Don't show or hide error screen if we're in connecting state.
      if (state == NET_STATE.CONNECTING && !opt_forceUpdate) {
        if (!this.connectingTimer_) {
          // First notification about CONNECTING state.
          this.clearConnectingTimer_();
          this.connectingTimer_ = window.setTimeout(
              this.updateState_.bind(
                  this, state, network, reason, lastNetworkType, true),
              CONNECTING_TIMEOUT_SEC * 1000);
        }
        return;
      }
      this.clearConnectingTimer_();

      var currentScreen = Oobe.getInstance().currentScreen;
      var offlineMessage = this;
      var isOnline = (state == NET_STATE.ONLINE);
      var isUnderCaptivePortal = (state == NET_STATE.PORTAL);
      var isProxyError = reason == ERROR_REASONS.PROXY_AUTH_CANCELLED ||
          reason == ERROR_REASONS.PROXY_CONNECTION_FAILED;
      var shouldOverlay = MANAGED_SCREENS.indexOf(currentScreen.id) != -1 &&
          !currentScreen.isLocal;
      var isTimeout = false;
      var isShown = !offlineMessage.classList.contains('hidden') &&
          !offlineMessage.classList.contains('faded');
      var currentScreenReloaded = false;

      // Reload frame if network is changed.
      if (reason == ERROR_REASONS.NETWORK_CHANGED) {
        if (state == NET_STATE.ONLINE && !currentScreenReloaded) {
          currentScreen.doReload();
          currentScreenReloaded = true;
        }
      }

      if (reason == ERROR_REASONS.PROXY_CONFIG_CHANGED && shouldOverlay &&
          !currentScreenReloaded) {
        // Schedules a immediate retry.
        currentScreen.doReload();
        currentScreenReloaded = true;
        console.log('Retry page load since proxy settings has been changed');
      }

      // Fake portal state for loading timeout.
      if (reason == ERROR_REASONS.LOADING_TIMEOUT) {
        isOnline = false;
        isUnderCaptivePortal = true;
        isTimeout = true;
      }

      // Portal was detected via generate_204 redirect on Chrome side.
      // Subsequent call to show dialog if it's already shown does nothing.
      if (reason == ERROR_REASONS.PORTAL_DETECTED) {
        isOnline = false;
        isUnderCaptivePortal = true;
      }

      if (!isOnline && shouldOverlay) {
        console.log('Show offline message: state=' + state +
                    ', network=' + network + ', reason=' + reason +
                    ', isUnderCaptivePortal=' + isUnderCaptivePortal);

        // Clear any error messages that might still be around.
        Oobe.clearErrors();

        offlineMessage.onBeforeShow(lastNetworkType);

        if (isUnderCaptivePortal && !isProxyError) {
          // Do not bother a user with obsessive captive portal showing. This
          // check makes captive portal being shown only once: either when error
          // screen is shown for the first time or when switching from another
          // error screen (offline, proxy).
          if (!isShown ||
              !offlineMessage.classList.contains('show-captive-portal')) {
            // In case of timeout we're suspecting that network might be
            // a captive portal but would like to check that first.
            // Otherwise (signal from shill / generate_204 got redirected)
            // show dialog right away.
            if (isTimeout)
              chrome.send('fixCaptivePortal');
            else
              chrome.send('showCaptivePortal');
          }
        } else {
          chrome.send('hideCaptivePortal');
        }

        if (isUnderCaptivePortal) {
          if (isProxyError) {
            offlineMessage.classList.remove('show-offline-message');
            offlineMessage.classList.remove('show-captive-portal');
            offlineMessage.classList.add('show-proxy-error');
          } else {
            $(CURRENT_NETWORK_NAME_ID).textContent = network;
            offlineMessage.classList.remove('show-offline-message');
            offlineMessage.classList.remove('show-proxy-error');
            offlineMessage.classList.add('show-captive-portal');
          }
        } else {
          offlineMessage.classList.remove('show-captive-portal');
          offlineMessage.classList.remove('show-proxy-error');
          offlineMessage.classList.add('show-offline-message');
        }

        offlineMessage.classList.remove('hidden');
        offlineMessage.classList.remove('faded');

        if (Oobe.getInstance().isNewOobe())
          Oobe.getInstance().updateInnerContainerSize_(offlineMessage);

        if (!currentScreen.classList.contains('faded')) {
          currentScreen.classList.add('faded');
          currentScreen.addEventListener('webkitTransitionEnd',
            function f(e) {
              currentScreen.removeEventListener('webkitTransitionEnd', f);
              if (currentScreen.classList.contains('faded'))
                currentScreen.classList.add('hidden');
            });
        }
        chrome.send('networkErrorShown');
        // Report back error screen UI being painted.
        window.webkitRequestAnimationFrame(function() {
          chrome.send('loginVisible');
        });
      } else {
        chrome.send('hideCaptivePortal');

        if (!offlineMessage.classList.contains('faded')) {
          console.log('Hide offline message. state=' + state +
                      ', network=' + network + ', reason=' + reason);

          offlineMessage.onBeforeHide();

          offlineMessage.classList.add('faded');
          if (offlineMessage.classList.contains('animated')) {
            offlineMessage.addEventListener('webkitTransitionEnd',
              function f(e) {
                offlineMessage.removeEventListener('webkitTransitionEnd', f);
                if (offlineMessage.classList.contains('faded'))
                  offlineMessage.classList.add('hidden');
              });
          } else {
            offlineMessage.classList.add('hidden');
          }

          currentScreen.classList.remove('hidden');
          currentScreen.classList.remove('faded');

          if (Oobe.getInstance().isNewOobe())
            Oobe.getInstance().updateInnerContainerSize_(currentScreen);

          // Forces a reload for Gaia screen on hiding error message.
          if (currentScreen.id == 'gaia-signin' && !currentScreenReloaded) {
            currentScreen.doReload();
            currentScreenReloaded = true;
          }
        } else if (currentScreen.id == 'gaia-signin' && currentScreen.loading) {
          if (!currentScreenReloaded) {
            currentScreen.doReload();
            currentScreenReloaded = true;
          }
        }
      }
    },

    // Request network state update with loading timeout as reason.
    showLoadingTimeoutError: function() {
      // Shows error message if it is not shown already.
      if (this.classList.contains('hidden')) {
        chrome.send('loginRequestNetworkState',
                    ['login.ErrorMessageScreen.updateState',
                     ERROR_REASONS.LOADING_TIMEOUT]);
      }
    }
  };

  /**
   * Network state changed callback.
   * @param {number} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the current network.
   * @param {string} reason Reason the callback was called.
   * @param {number} lastNetworkType Last active network type.
   */
  ErrorMessageScreen.updateState = function(
      state, network, reason, lastNetworkType) {
    $('error-message').updateState_(state, network, reason, lastNetworkType);
  };

  /**
   * Handler for iframe's error notification coming from the outside.
   * For more info see C++ class 'SnifferObserver' which calls this method.
   * @param {number} error Error code.
   */
  ErrorMessageScreen.onFrameError = function(error) {
    console.log('Gaia frame error = ' + error);
    if (error == NET_ERROR.ABORTED_BY_USER) {
      // Gaia frame was reloaded. Nothing to do here.
      return;
    }
    $('gaia-signin').onFrameError(error);
    // Check current network state if currentScreen is a managed one.
    var currentScreen = Oobe.getInstance().currentScreen;
    if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1) {
      chrome.send('loginRequestNetworkState',
                  ['login.ErrorMessageScreen.maybeRetry',
                   'frame error:' + error]);
    }
  };

  /**
   * Network state callback where we decide whether to schedule a retry.
   */
  ErrorMessageScreen.maybeRetry =
      function(state, network, reason, lastNetworkType) {
    console.log('ErrorMessageScreen.maybeRetry, state=' + state +
                ', network=' + network +
                ', lastNetworkType=' + lastNetworkType);

    // No retry if we are not online.
    if (state != NET_STATE.ONLINE)
      return;

    var currentScreen = Oobe.getInstance().currentScreen;
    if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1 &&
        state != NET_STATE.CONNECTING) {
      this.updateState(NET_STATE.PORTAL, network, reason, lastNetworkType);
      // Schedules a retry.
      currentScreen.scheduleRetry();
    }
  };

  /**
   * Updates screen localized content like links since they're not updated
   * via template.
   */
  ErrorMessageScreen.updateLocalizedContent = function() {
    $('error-message').updateLocalizedContent_();
  };

  return {
    ErrorMessageScreen: ErrorMessageScreen
  };
});
