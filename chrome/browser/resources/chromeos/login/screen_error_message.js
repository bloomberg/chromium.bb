// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
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
    Oobe.getInstance().registerScreen(screen);
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
        var gaiaScreen = $(SCREEN_GAIA_SIGNIN);
        // Schedules a immediate retry.
        gaiaScreen.doReload();
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

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      cr.ui.Oobe.clearErrors();
      var lastNetworkType = data['lastNetworkType'] || 0;
      cr.ui.DropDown.show('offline-networks-list', false, lastNetworkType);
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
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
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
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
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Prepares error screen to show offline error.
     * @private
     */
    showOfflineError_: function() {
      this.classList.remove('show-captive-portal');
      this.classList.remove('show-proxy-error');
      this.classList.add('show-offline-message');
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Prepares error screen to show offline login link.
     * @private
     */
    allowOfflineLogin_: function(allowed) {
      if (allowed)
        this.classList.add('allow-offline-login');
      else
        this.classList.remove('allow-offline-login');
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
    * Prepares error screen to show offline login link.
    * @param {boolean} allowed True if login link should be visible.
    */
  ErrorMessageScreen.allowOfflineLogin = function(allowed) {
    $('error-message').allowOfflineLogin_(allowed);
  };

  ErrorMessageScreen.onBeforeShow = function(lastNetworkType) {
    $('error-message').onBeforeShow(lastNetworkType);
  };

  ErrorMessageScreen.onBeforeHide = function() {
    $('error-message').onBeforeHide();
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
