// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  // Screens that should have offline message overlay.
  /** @const */ var MANAGED_SCREENS = [SCREEN_GAIA_SIGNIN];

  // Network state constants.
  var NET_STATE = {
    OFFLINE: 0,
    ONLINE: 1,
    PORTAL: 2,
    CONNECTING: 3,
    UNKNOWN: 4
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

    /** @override */
    decorate: function() {
      cr.ui.DropDown.decorate($('offline-networks-list'));
      this.updateLocalizedContent_();
    },

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

      $('error-guest-signin').hidden = $('guestSignin').hidden;

      $('error-offline-login').hidden = !currentScreen.isOfflineAllowed;
    },

    onBeforeHide: function() {
      cr.ui.DropDown.hide('offline-networks-list');
    },

    /**
     * Prepares error screen to show proxy error.
     * @private
     */
    showProxyError_: function() {
      this.classList.remove('show-offline-message');
      this.classList.remove('show-captive-portal');
      this.classList.add('show-proxy-error');
    },

    /**
     * Prepares error screen to show captive portal error.
     * @param {string} network Name of the current network
     * @private
     */
    showCaptivePortalError_: function(network) {
      $(CURRENT_NETWORK_NAME_ID).textContent = network;
      this.classList.remove('show-offline-message');
      this.classList.remove('show-proxy-error');
      this.classList.add('show-captive-portal');
    },

    /**
     * Prepares error screen to show offline error.
     * @private
     */
    showOfflineError_: function() {
      this.classList.remove('show-captive-portal');
      this.classList.remove('show-proxy-error');
      this.classList.add('show-offline-message');
    },

    /**
     * Shows screen.
     * @param {object} screen Screen that should be shown.
     * @private
     */
    showScreen_: function(screen) {
      screen.classList.remove('hidden');
      screen.classList.remove('faded');

      if (Oobe.getInstance().isNewOobe())
        Oobe.getInstance().updateScreenSize(screen);
    },

    /**
     * Shows/hides offline message.
     * @param {boolean} visible True to show offline message.
     * @private
     */
    showOfflineMessage_: function(visible) {
      var offlineMessage = this;
      var currentScreen = Oobe.getInstance().currentScreen;

      if (visible) {
        this.showScreen_(offlineMessage);

        if (!currentScreen.classList.contains('faded')) {
          currentScreen.classList.add('faded');
          currentScreen.addEventListener('webkitTransitionEnd',
            function f(e) {
              currentScreen.removeEventListener('webkitTransitionEnd', f);
              if (currentScreen.classList.contains('faded'))
                currentScreen.classList.add('hidden');
            });
        }

        // Report back error screen UI being painted.
        window.webkitRequestAnimationFrame(function() {
          chrome.send('loginVisible', ['network-error']);
        });
      } else {
        this.showScreen_(currentScreen);

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
      }
    },
  };

  /**
   * Prepares error screen to show proxy error.
   */
  ErrorMessageScreen.showProxyError = function() {
    $('error-message').showProxyError_();
  };

  /**
   * Prepares error screen to show captive portal error.
   * @param {string} network Name of the current network
   */
  ErrorMessageScreen.showCaptivePortalError = function(network) {
    $('error-message').showCaptivePortalError_(network);
  };

  /**
   * Prepares error screen to show offline error.
   */
  ErrorMessageScreen.showOfflineError = function() {
    $('error-message').showOfflineError_();
  };

  /**
   * Shows/hides offline message.
   * @param {boolean} visible True to show offline message.
   */
  ErrorMessageScreen.showOfflineMessage = function(visible) {
    $('error-message').showOfflineMessage_(visible);
  };

  ErrorMessageScreen.onBeforeShow = function(lastNetworkType) {
    $('error-message').onBeforeShow(lastNetworkType);
  };

  ErrorMessageScreen.onBeforeHide = function() {
    $('error-message').onBeforeHide();
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
      chrome.send('updateState',
                  [NET_STATE.PORTAL, network, reason, lastNetworkType]);
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
