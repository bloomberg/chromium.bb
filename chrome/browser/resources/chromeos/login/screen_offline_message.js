 // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  // Screens that should have offline message overlay.
  const MANAGED_SCREENS = ['gaia-signin', 'signin'];

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
      window.addEventListener('online',
                              this.handleNetworkStateChange_.bind(this));
      window.addEventListener('offline',
                              this.handleNetworkStateChange_.bind(this));
      cr.ui.DropDown.decorate($('offline-networks-list'));
    },

    onBeforeShow: function() {
      cr.ui.DropDown.setActive('offline-networks-list', true);
    },

    onBeforeHide: function() {
      cr.ui.DropDown.setActive('offline-networks-list', false);
    },

    /**
     * Shows or hides offline message based on network on/offline state.
     */
    update: function() {
      var currentScreen = Oobe.getInstance().currentScreen;
      var offlineMessage = this;
      var isOffline = !window.navigator.onLine;
      var shouldOverlay = MANAGED_SCREENS.indexOf(currentScreen.id) != -1;

      if (isOffline && shouldOverlay) {
        offlineMessage.onBeforeShow();

        $('offline-message-text').hidden = false;
        $('captive-portal-message-text').hidden = true;

        offlineMessage.classList.remove('hidden');
        offlineMessage.classList.remove('faded');

        if (!currentScreen.classList.contains('faded')) {
          currentScreen.classList.add('faded');
          currentScreen.addEventListener('webkitTransitionEnd',
            function f(e) {
              currentScreen.removeEventListener('webkitTransitionEnd', f);
              currentScreen.classList.add('hidden');
            });
        }
      } else {
        if (!offlineMessage.classList.contains('faded')) {
          offlineMessage.onBeforeHide();

          offlineMessage.classList.add('faded');
          offlineMessage.addEventListener('webkitTransitionEnd',
            function f(e) {
              offlineMessage.removeEventListener('webkitTransitionEnd', f);
              offlineMessage.classList.add('hidden');
            });

          currentScreen.classList.remove('hidden');
          currentScreen.classList.remove('faded');

          // Triggers refresh for Gaia sign-in screen.
          if (currentScreen.id == 'gaia-signin')
            chrome.send('showAddUser');
        }
      }
    },

    /**
     * Handler of online/offline event.
     */
    handleNetworkStateChange_: function() {
      this.update();
    }
  };

  /**
   * Handler for iframe's error notification coming from the outside.
   * For more info see C++ class 'SnifferObserver' which calls this method.
   * @param {number} error Error code.
   */
  OfflineMessageScreen.onFrameError = function(error) {
    var currentScreen = Oobe.getInstance().currentScreen;
    var offlineMessage = $('offline-message');
    var isOffline = !window.navigator.onLine;
    var shouldOverlay = MANAGED_SCREENS.indexOf(currentScreen.id) != -1;

    if (!shouldOverlay)
      return;

    if (!isOffline) {
      $('offline-message-text').hidden = true;
      $('captive-portal-message-text').hidden = false;
    }

    if (!currentScreen.classList.contains('faded')) {
      offlineMessage.onBeforeShow();

      offlineMessage.classList.remove('hidden');
      offlineMessage.classList.remove('faded');

      currentScreen.classList.add('faded');
      currentScreen.addEventListener('webkitTransitionEnd',
        function f(e) {
          currentScreen.removeEventListener('webkitTransitionEnd', f);
          currentScreen.classList.add('hidden');
        });
    }
  };

  return {
    OfflineMessageScreen: OfflineMessageScreen
  };
});
