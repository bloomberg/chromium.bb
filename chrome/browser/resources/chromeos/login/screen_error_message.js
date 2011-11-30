 // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  // Screens that should have offline message overlay.
  const MANAGED_SCREENS = ['gaia-signin'];

  // Network state constants.
  const NET_STATE = {
    OFFLINE: 0,
    ONLINE: 1,
    PORTAL: 2
  };

  // Error reasons which are passed to updateState_() method.
  const ERROR_REASONS = {
    PROXY_AUTH_CANCELLED: 'frame error:111',
    PROXY_CONNECTION_FAILED: 'frame error:130'
  };

  // Frame loading errors.
  const NET_ERROR = {
    ABORTED_BY_USER: 3
  };

  // Link which starts guest session for captive portal fixing.
  const FIX_CAPTIVE_PORTAL_ID = 'captive-portal-fix-link';

  // Id of the element which holds current network name.
  const CURRENT_NETWORK_NAME_ID = 'captive-portal-network-name';

  // Link which triggers frame reload.
  const RELOAD_PAGE_ID = 'proxy-error-retry-link';

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
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent_: function() {
      $('captive-portal-message-text').innerHTML = localStrings.getStringF(
        'captivePortalMessage',
        '<b id="' + CURRENT_NETWORK_NAME_ID + '"></b>',
        '<a id="' + FIX_CAPTIVE_PORTAL_ID + '" class="signin-link" href="#">',
        '</a>');
      $(FIX_CAPTIVE_PORTAL_ID).onclick = function() {
        chrome.send('fixCaptivePortal');
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

      // TODO(altimofeev): Support offline sign-in as well.
      $('error-guest-signin').innerHTML = localStrings.getStringF(
          'guestSignin',
          '<a id="error-guest-signin-link" class="signin-link" href="#">',
          '</a>');
      $('error-guest-signin-link').onclick = function() {
        chrome.send('launchIncognito');
      };
    },

    onBeforeShow: function(lastNetworkType) {
      cr.ui.DropDown.show('offline-networks-list', false, lastNetworkType);

      $('error-guest-signin').hidden = $('guestSignin').hidden ||
          !$('add-user-header-bar-item').hidden;
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
     * Shows or hides offline message based on network on/offline state.
     * @param {Integer} state Current state of the network (see NET_STATE).
     * @param {string} network Name of the current network.
     * @param {string} reason Reason the callback was called.
     * @param {int} lastNetworkType Last active network type.
     */
    updateState_: function(state, network, reason, lastNetworkType) {
      var currentScreen = Oobe.getInstance().currentScreen;
      var offlineMessage = this;
      var isOnline = (state == NET_STATE.ONLINE);
      var isUnderCaptivePortal = (state == NET_STATE.PORTAL);
      var isProxyError = reason == ERROR_REASONS.PROXY_AUTH_CANCELLED ||
          reason == ERROR_REASONS.PROXY_CONNECTION_FAILED;
      var shouldOverlay = MANAGED_SCREENS.indexOf(currentScreen.id) != -1;

      if (reason == 'proxy changed' && shouldOverlay &&
          !offlineMessage.classList.contains('hidden') &&
          offlineMessage.classList.contains('show-captive-portal')) {
        // Schedules a immediate retry.
        currentScreen.doReload();
        console.log('Retry page load since proxy settings has been changed');
      }

      if (!isOnline && shouldOverlay) {
        console.log('Show offline message, state=' + state +
                    ', network=' + network +
                    ', isUnderCaptivePortal=' + isUnderCaptivePortal);
        offlineMessage.onBeforeShow(lastNetworkType);

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

        if (!currentScreen.classList.contains('faded')) {
          currentScreen.classList.add('faded');
          currentScreen.addEventListener('webkitTransitionEnd',
            function f(e) {
              currentScreen.removeEventListener('webkitTransitionEnd', f);
              if (currentScreen.classList.contains('faded'))
                currentScreen.classList.add('hidden');
            });
        }
      } else {
        if (!offlineMessage.classList.contains('faded')) {
          console.log('Hide offline message.');
          offlineMessage.onBeforeHide();

          offlineMessage.classList.add('faded');
          offlineMessage.addEventListener('webkitTransitionEnd',
            function f(e) {
              offlineMessage.removeEventListener('webkitTransitionEnd', f);
              if (offlineMessage.classList.contains('faded'))
                offlineMessage.classList.add('hidden');
            });

          currentScreen.classList.remove('hidden');
          currentScreen.classList.remove('faded');
        }
      }
    },
  };

  /**
   * Network state changed callback.
   * @param {Integer} state Current state of the network (see NET_STATE).
   * @param {string} network Name of the current network.
   * @param {string} reason Reason the callback was called.
   * @param {int} lastNetworkType Last active network type.
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
    // Offline and simple captive portal cases are handled by the
    // NetworkStateInformer, so only the case when browser is online is
    // valuable.
    if (window.navigator.onLine) {
      // Check current network state if currentScreen is a managed one.
      var currentScreen = Oobe.getInstance().currentScreen;
      if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1) {
        chrome.send('loginRequestNetworkState',
                    ['login.ErrorMessageScreen.maybeRetry',
                     'frame error:' + error]);
      }
    }
  };

  /**
   * Network state callback where we decide whether to schdule a retry.
   */
  ErrorMessageScreen.maybeRetry = function(state, network, reason) {
    console.log('ErrorMessageScreen.maybeRetry, state=' + state +
                ', network=' + network);

    // No retry if we are not online.
    if (state != NET_STATE.ONLINE)
      return;

    var currentScreen = Oobe.getInstance().currentScreen;
    if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1) {
      this.updateState(NET_STATE.PORTAL, network, reason);
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
