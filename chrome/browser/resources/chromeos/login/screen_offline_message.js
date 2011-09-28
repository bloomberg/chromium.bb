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

  // Link which starts guest session for captive portal fixing.
  const FIX_CAPTIVE_PORTAL_ID = 'captive-portal-fix-link';

  // Id of the element which holds current network name.
  const CURRENT_NETWORK_NAME_ID = 'captive-portal-network-name';

  /**
   * Creates a new offline message screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var OfflineMessageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  OfflineMessageScreen.register = function() {
    var screen = $('offline-message');
    OfflineMessageScreen.decorate(screen);

    // Note that OfflineMessageScreen is not registered with Oobe because
    // it is shown on top of sign-in screen instead of as an independent screen.
  };

  OfflineMessageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      chrome.send('loginAddNetworkStateObserver',
                  ['login.OfflineMessageScreen.updateState']);

      cr.ui.DropDown.decorate($('offline-networks-list'));

      $('captive-portal-message-text').innerHTML = localStrings.getStringF(
          'captivePortalMessage',
          '<b id="' + CURRENT_NETWORK_NAME_ID + '"></b>',
          '<a id="' + FIX_CAPTIVE_PORTAL_ID + '" class="signin-link" href="#">',
          '</a>');
      $(FIX_CAPTIVE_PORTAL_ID).onclick = function() {
        chrome.send('fixCaptivePortal');
      };
    },

    onBeforeShow: function() {
      cr.ui.DropDown.setActive('offline-networks-list', true);
    },

    onBeforeHide: function() {
      cr.ui.DropDown.setActive('offline-networks-list', false);
    },

    update: function() {
      chrome.send('loginRequestNetworkState',
                  ['login.OfflineMessageScreen.updateState']);
    },

    /**
     * Shows or hides offline message based on network on/offline state.
     */
    updateState: function(state, network) {
      var currentScreen = Oobe.getInstance().currentScreen;
      var offlineMessage = this;
      var isOnline = (state == NET_STATE.ONLINE);
      var isUnderCaptivePortal = (state == NET_STATE.PORTAL);
      var shouldOverlay = MANAGED_SCREENS.indexOf(currentScreen.id) != -1;

      if (!isOnline && shouldOverlay) {
        console.log('Show offline message, state=' + state +
                    ', network=' + network +
                    ', isUnderCaptivePortal=' + isUnderCaptivePortal);
        offlineMessage.onBeforeShow();

        if (isUnderCaptivePortal) {
          $(CURRENT_NETWORK_NAME_ID).innerText = network;
          offlineMessage.classList.remove('show-offline-message');
          offlineMessage.classList.add('show-captive-portal');
        } else {
          offlineMessage.classList.remove('show-captive-portal');
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
   * @param {Integer} state Current state of the network: 0 - offline;
   * 1 - online; 2 - under the captive portal.
   * @param {string} network Name of the current network.
   */
  OfflineMessageScreen.updateState = function(state, network) {
    $('offline-message').updateState(state, network);
  };

  /**
   * Handler for iframe's error notification coming from the outside.
   * For more info see C++ class 'SnifferObserver' which calls this method.
   * @param {number} error Error code.
   */
  OfflineMessageScreen.onFrameError = function(error) {
    console.log('Gaia frame error = ' + error);
    // Offline and simple captive portal cases are handled by the
    // NetworkStateInformer, so only the case when browser is online is
    // valuable.
    if (window.navigator.onLine) {
      // Check current network state if currentScreen is a managed one.
      var currentScreen = Oobe.getInstance().currentScreen;
      if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1) {
        chrome.send('loginRequestNetworkState',
                    ['login.OfflineMessageScreen.maybeRetry']);
      }
    }
  };

  /**
   * Network state callback where we decide whether to schdule a retry.
   */
  OfflineMessageScreen.maybeRetry = function(state, network) {
    console.log('OfflineMessageScreen.maybeRetry, state=' + state +
                ', network=' + network);

    // No retry if we are not online.
    if (state != NET_STATE.ONLINE)
      return;

    var currentScreen = Oobe.getInstance().currentScreen;
    if (MANAGED_SCREENS.indexOf(currentScreen.id) != -1) {
      this.updateState(NET_STATE.PORTAL, network);
      // Schedules a retry.
      currentScreen.schdeduleRetry();
    }
  };

  return {
    OfflineMessageScreen: OfflineMessageScreen
  };
});
