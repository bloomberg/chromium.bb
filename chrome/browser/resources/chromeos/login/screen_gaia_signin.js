// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

cr.define('login', function() {

  /**
   * Creates a new sign in screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var GaiaSigninScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  GaiaSigninScreen.register = function() {
    var screen = $('gaia-signin');
    GaiaSigninScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
    window.addEventListener('message',
                            screen.onMessage_.bind(screen), false);
  };

  GaiaSigninScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Authentication extension's start page URL.
    extension_url_: null,

    // Number of times that we reload extension frame.
    retryCount_: 0,

    // Timer id of pending retry.
    retryTimer_: undefined,

    /** @inheritDoc */
    decorate: function() {
      $('createAccount').innerHTML = localStrings.getStringF(
          'createAccount',
          '<a id="createAccountLink" class="signin-link" href="#">',
          '</a>');
      $('guestSignin').innerHTML = localStrings.getStringF(
          'guestSignin',
          '<a id="guestSigninLink" class="signin-link" href="#">',
          '</a>');
      $('createAccountLink').onclick = function() {
        chrome.send('createAccount');
      };
      $('guestSigninLink').onclick = function() {
        chrome.send('launchIncognito');
      };
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('signinScreenTitle');
    },

    /**
     * Shows/hides loading UI.
     * @param {boolean} show True to show loading UI.
     * @private
     */
    showLoadingUI_: function(show) {
      $('gaia-loading').hidden = !show;
      $('signin-frame').hidden = show;

      // Sign-in right panel is hidden if all its items are hidden.
      $('signin-right').hidden = show ||
          ($('createAccount').hidden && $('guestSignin').hidden);
    },

    /**
     * Whether Gaia is loading.
     * @type {boolean}
     */
    get loading() {
      return !$('gaia-loading').hidden;
    },
    set loading(loading) {
      if (loading == this.loading)
        return;

      this.showLoadingUI_(loading);
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param data {string} Screen init payload. Url of auth extension start
     *        page.
     */
    onBeforeShow: function(data) {
      console.log('Opening extension: ' + data.startUrl +
                  ', opt_email=' + data.email);

      var params = [];
      if (data.gaiaOrigin)
        params.push('gaiaOrigin=' + encodeURIComponent(data.gaiaOrigin));
      if (data.hl)
        params.push('hl=' + encodeURIComponent(data.hl));
      if (data.email)
        params.push('email=' + encodeURIComponent(data.email));
      if (data.test_email)
        params.push('test_email=' + encodeURIComponent(data.test_email));
      if (data.test_password)
        params.push('test_password=' + encodeURIComponent(data.test_password));

      var url = data.startUrl;
      if (params.length)
        url += '?' + params.join('&');

      $('signin-frame').src = url;
      this.extension_url_ = url;

      $('createAccount').hidden = !data.createAccount;
      $('guestSignin').hidden = !data.guestSignin;

      // Announce the name of the screen, if accessibility is on.
      $('gaia-signin-aria-label').setAttribute(
          'aria-label', localStrings.getString('signinScreenTitle'));

      // Button header is always visible when sign in is presented.
      // Header is hidden once GAIA reports on successful sign in.
      Oobe.getInstance().headerHidden = false;

      this.loading = true;
      this.clearRetry_();
    },

    /**
     * Checks if message comes from the loaded authentication extension.
     * @param e {object} Payload of the received HTML5 message.
     * @type {bool}
     */
    isAuthExtMessage_: function(e) {
      return this.extension_url_ != null &&
          this.extension_url_.indexOf(e.origin) == 0 &&
          e.source == $('signin-frame').contentWindow;
    },

    /**
     * Event handler that is invoked when HTML5 message is received.
     * @param e {object} Payload of the received HTML5 message.
     */
    onMessage_: function(e) {
      var msg = e.data;
      if (msg.method == 'completeLogin' && this.isAuthExtMessage_(e)) {
        chrome.send('completeLogin', [msg.email, msg.password] );
        this.loading = true;
        // Now that we're in logged in state header should be hidden.
        Oobe.getInstance().headerHidden = true;
      } else if (msg.method == 'loginUILoaded' && this.isAuthExtMessage_(e)) {
        $('offline-message').update();
        this.loading = false;
        this.clearRetry_();
        chrome.send('loginWebuiReady');
      }
    },

    /**
     * Clears input fields and switches to input mode.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      // Reload and show the sign-in UI if needed.
      if (takeFocus)
        Oobe.showSigninUI();
    },

    /**
     * Clears retry data.
     * @private
     */
    clearRetry_: function() {
      this.retryCount_ = 0;
      if (this.retryTimer_) {
        window.clearTimeout(this.retryTimer_);
        this.retryTimer_ = undefined;
      }
    },

    /**
     * Reloads extension frame.
     * @private
     */
    doReload_: function() {
      console.log('Reload auth extension frame.');
      $('signin-frame').src = this.extension_url_;
      this.retryTimer_ = undefined;
    },

    /**
     * Schedules extension frame reload.
     */
    schdeduleRetry: function() {
      if (this.retryCount_ >= 3 || this.retryTimer_)
        return;

      const MAX_DELAY = 7200;  // 7200 seconds (i.e. 2 hours)
      const MIN_DELAY = 1;  // 1 second

      var delay = Math.pow(2, this.retryCount_) * 5;
      delay = Math.max(MIN_DELAY, Math.min(MAX_DELAY, delay)) * 1000;

      ++this.retryCount_;
      this.retryTimer_ = window.setTimeout(this.doReload_.bind(this), delay);
      console.log('GaiaSigninScreen schdeduleRetry in ' + delay + 'ms.');
    }
  };

  return {
    GaiaSigninScreen: GaiaSigninScreen
  };
});
