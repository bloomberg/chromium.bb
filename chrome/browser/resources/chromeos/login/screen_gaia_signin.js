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

    // Frame loading error code (0 - no error).
    error_: 0,

    // Authentication extension's start page URL.
    extensionUrl_: null,

    // Whether extension should be loaded silently.
    silentLoad_: false,

    // Whether there is focused element.
    hasFocused_: false,

    // Number of times that we reload extension frame.
    retryCount_: 0,

    // Timer id of pending retry.
    retryTimer_: undefined,

    /** @inheritDoc */
    decorate: function() {
      this.frame_ = $('signin-frame');

      this.updateLocalizedContent();

      document.addEventListener(
          'focusin', this.selfBind_(this.onFocusIn_.bind(this)));
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
      this.frame_.hidden = show;

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
      // Announce the name of the screen, if accessibility is on.
      $('gaia-signin-aria-label').setAttribute(
          'aria-label', localStrings.getString('signinScreenTitle'));

      // Button header is always visible when sign in is presented.
      // Header is hidden once GAIA reports on successful sign in.
      Oobe.getInstance().headerHidden = false;
    },

    /**
     * Returns function which gets an event and passes it and self to listener.
     * @param {!Object} listener Listener to be wrapped.
     */
    selfBind_: function(listener) {
      var selfBinded = function(e) {
        listener(e, selfBinded);
      }
      return selfBinded;
    },

    /**
     * Tracks first focus in event.
     * @param {!Object} e Focus in event.
     * @param {!Object} listener Listener which shold be removed from event
     *   listeners list.
     */
    onFocusIn_: function(e, listener) {
      this.hasFocused_ = true;
      document.removeEventListener('focusin', listener);
    },

    /**
     * Restore focus back to the focused element.
     * @param {!Object} e Focus out event.
     * @param {!Object} listener Listener which shold be removed from event
     *   listeners list.
     */
    onFocusOut_: function(e, listener) {
      window.setTimeout(e.target.focus.bind(e.target), 0);
      document.removeEventListener('focusout', listener);
    },

    loadAuthExtension_: function(data) {
      this.silentLoad_ = data.silentLoad;

      $('createAccount').hidden = !data.createAccount;
      $('guestSignin').hidden = !data.guestSignin;

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

      if (data.forceReload || this.extensionUrl_ != url) {
        console.log('Opening extension: ' + data.startUrl +
                    ', opt_email=' + data.email);

        this.error_ = 0;
        this.frame_.src = url;
        this.extensionUrl_ = url;

        this.loading = true;
        this.clearRetry_();
      } else if (this.loading) {
        if (this.error_) {
          // An error has occurred, so trying to reload.
          this.doReload();
        } else {
          console.log('Gaia is still loading.');
          // Nothing to do here. Just wait until the extension loads.
        }
      } else {
        // TODO(altimofeev): GAIA extension is reloaded to make focus be set
        // correctly. When fix on the GAIA side is ready, this reloading should
        // be deleted.
        this.doReload();
      }
    },

    /**
     * Checks if message comes from the loaded authentication extension.
     * @param e {object} Payload of the received HTML5 message.
     * @type {bool}
     */
    isAuthExtMessage_: function(e) {
      return this.extensionUrl_ != null &&
          this.extensionUrl_.indexOf(e.origin) == 0 &&
          e.source == this.frame_.contentWindow;
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
        // TODO(altimofeev): there is no guarantee that next 'focusout' event
        // will be caused by the extension, so better approach is direct asking
        // the extension (and gaia consequently) to not grab the focus.
        if (this.silentLoad_ && this.hasFocused_) {
          document.addEventListener(
              'focusout', this.selfBind_(this.onFocusOut_.bind(this)));
        }
        $('error-message').update();
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
     */
    doReload: function() {
      console.log('Reload auth extension frame.');
      this.error_ = 0;
      this.frame_.src = this.extensionUrl_;
      this.retryTimer_ = undefined;
      this.loading = true;
    },

    /**
     * Schedules extension frame reload.
     */
    scheduleRetry: function() {
      if (this.retryCount_ >= 3 || this.retryTimer_)
        return;

      const MAX_DELAY = 7200;  // 7200 seconds (i.e. 2 hours)
      const MIN_DELAY = 1;  // 1 second

      var delay = Math.pow(2, this.retryCount_) * 5;
      delay = Math.max(MIN_DELAY, Math.min(MAX_DELAY, delay)) * 1000;

      ++this.retryCount_;
      this.retryTimer_ = window.setTimeout(this.doReload.bind(this), delay);
      console.log('GaiaSigninScreen scheduleRetry in ' + delay + 'ms.');
    },

    /**
     * This method is called when a frame loading error appears.
     * @param {int} error Error code.
     */
    onFrameError: function(error) {
      this.error_ = error;
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
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
    }
  };

  GaiaSigninScreen.loadAuthExtension = function(data) {
    $('gaia-signin').loadAuthExtension_(data);
  };

  return {
    GaiaSigninScreen: GaiaSigninScreen
  };
});
