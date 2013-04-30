// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

<include src="../../gaia_auth_host/gaia_auth_host.js"></include>

login.createScreen('GaiaSigninScreen', 'gaia-signin', function() {
  // Gaia loading time after which error message must be displayed and
  // lazy portal check should be fired.
  /** @const */ var GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC = 7;

  // Maximum Gaia loading time in seconds.
  /** @const */ var MAX_GAIA_LOADING_TIME_SEC = 60;

  // Frame loading errors.
  /** @const */ var NET_ERROR = {
    ABORTED_BY_USER: 3
  };

  return {
    EXTERNAL_API: [
      'loadAuthExtension',
      'updateAuthExtension',
      'doReload',
      'onFrameError',
    ],

    /**
     * Frame loading error code (0 - no error).
     * @type {number}
     * @private
     */
    error_: 0,

    /**
     * Saved gaia auth host load params.
     * @type {?string}
     * @private
     */
    gaiaAuthParams_: null,

    /**
     * Whether extension should be loaded silently.
     * @type {boolean}
     * @private
     */
    silentLoad_: false,

    /**
     * Whether local version of Gaia page is used.
     * @type {boolean}
     * @private
     */
    isLocal_: false,

    /**
     * Email of the user, which is logging in using offline mode.
     * @type {string}
     */
    email: '',

    /**
     * Timer id of pending load.
     * @type {number}
     * @private
     */
    loadingTimer_: undefined,

    /**
     * Whether user can cancel Gaia screen.
     * @type {boolean}
     * @private
     */
    cancelAllowed_: undefined,

    /** @override */
    decorate: function() {
      this.gaiaAuthHost_ = new cr.login.GaiaAuthHost($('signin-frame'));
      this.gaiaAuthHost_.addEventListener('ready',
                                          this.onAuthReady_.bind(this));

      this.updateLocalizedContent();
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('signinScreenTitle');
    },

    /**
     * Returns true if local version of Gaia is used.
     * @type {boolean}
     */
    get isLocal() {
      return this.isLocal_;
    },

    /**
     * Sets whether local version of Gaia is used.
     * @param {boolean} value Whether local version of Gaia is used.
     */
    set isLocal(value) {
      this.isLocal_ = value;
      chrome.send('updateOfflineLogin', [value]);
    },

    /**
     * Shows/hides loading UI.
     * @param {boolean} show True to show loading UI.
     * @private
     */
    showLoadingUI_: function(show) {
      $('gaia-loading').hidden = !show;
      this.gaiaAuthHost_.frame.hidden = show;
      $('signin-right').hidden = show;
      $('enterprise-info-container').hidden = show;
      $('gaia-signin-divider').hidden = show;
    },

    /**
     * Handler for Gaia loading suspiciously long timeout.
     * @private
     */
    onLoadingSuspiciouslyLong_: function() {
      if (this != Oobe.getInstance().currentScreen)
        return;
      chrome.send('showLoadingTimeoutError');
      this.loadingTimer_ = window.setTimeout(
          this.onLoadingTimeOut_.bind(this),
          (MAX_GAIA_LOADING_TIME_SEC - GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC) *
          1000);
    },

    /**
     * Handler for Gaia loading timeout.
     * @private
     */
    onLoadingTimeOut_: function() {
      this.loadingTimer_ = undefined;
      chrome.send('showLoadingTimeoutError');
    },

    /**
     * Clears loading timer.
     * @private
     */
    clearLoadingTimer_: function() {
      if (this.loadingTimer_) {
        window.clearTimeout(this.loadingTimer_);
        this.loadingTimer_ = undefined;
      }
    },

    /**
     * Sets up loading timer.
     * @private
     */
    startLoadingTimer_: function() {
      this.clearLoadingTimer_();
      this.loadingTimer_ = window.setTimeout(
          this.onLoadingSuspiciouslyLong_.bind(this),
          GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC * 1000);
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
     * @param {string} data Screen init payload. Url of auth extension start
     *                      page.
     */
    onBeforeShow: function(data) {
      chrome.send('loginUIStateChanged', ['gaia-signin', true]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.GAIA_SIGNIN;

      // Announce the name of the screen, if accessibility is on.
      $('gaia-signin-aria-label').setAttribute(
          'aria-label', loadTimeData.getString('signinScreenTitle'));

      // Button header is always visible when sign in is presented.
      // Header is hidden once GAIA reports on successful sign in.
      Oobe.getInstance().headerHidden = false;
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      chrome.send('loginUIStateChanged', ['gaia-signin', false]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Loads the authentication extension into the iframe.
     * @param {Object} data Extension parameters bag.
     * @private
     */
    loadAuthExtension: function(data) {
      this.silentLoad_ = data.silentLoad;
      this.isLocal = data.isLocal;
      this.email = '';

      this.updateAuthExtension(data);

      var params = {};
      for (var i in cr.login.GaiaAuthHost.SUPPORTED_PARAMS) {
        var name = cr.login.GaiaAuthHost.SUPPORTED_PARAMS[i];
        if (data[name])
          params[name] = data[name];
      }

      if (data.localizedStrings)
        params.localizedStrings = data.localizedStrings;

      if (data.forceReload ||
          JSON.stringify(this.gaiaAuthParams_) != JSON.stringify(params)) {
        this.error_ = 0;
        this.gaiaAuthHost_.load(data.useOffline,
                                params,
                                this.onAuthCompleted_.bind(this));
        this.gaiaAuthParams_ = params;

        this.loading = true;
        this.startLoadingTimer_();
      } else if (this.loading && this.error_) {
        // An error has occurred, so trying to reload.
        this.doReload();
      }
    },

    /**
     * Updates the authentication extension with new parameters, if needed.
     * @param {Object} data New extension parameters bag.
     * @private
     */
    updateAuthExtension: function(data) {
      var reasonLabel = $('gaia-signin-reason');
      if (data.passwordChanged) {
        reasonLabel.textContent =
            loadTimeData.getString('signinScreenPasswordChanged');
        reasonLabel.hidden = false;
      } else {
        reasonLabel.hidden = true;
      }

      $('createAccount').hidden = !data.createAccount;
      $('guestSignin').hidden = !data.guestSignin;
      $('createManagedUserPane').hidden = !data.createLocallyManagedUser;
      // Allow cancellation of screen only when user pods can be displayed.
      this.cancelAllowed_ = data.isShowUsers && $('pod-row').pods.length;
      $('login-header-bar').allowCancel = this.cancelAllowed_;

      // Sign-in right panel is hidden if all of its items are hidden.
      var noRightPanel = $('gaia-signin-reason').hidden &&
                         $('createAccount').hidden &&
                         $('guestSignin').hidden &&
                         $('createManagedUserPane').hidden;
      this.classList.toggle('no-right-panel', noRightPanel);
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Invoked when the auth host emits 'ready' event.
     * @private
     */
    onAuthReady_: function() {
      this.loading = false;
      chrome.send('loginScreenUpdate');
      this.clearLoadingTimer_();

      // Show deferred error bubble.
      if (this.errorBubble_) {
        this.showErrorBubble(this.errorBubble_[0], this.errorBubble_[1]);
        this.errorBubble_ = undefined;
      }

      chrome.send('loginWebuiReady');
      chrome.send('loginVisible', ['gaia-signin']);

      // Warm up the user images screen.
      Oobe.getInstance().preloadScreen({id: SCREEN_USER_IMAGE_PICKER});
    },

    /**
     * Invoked when auth is completed successfully.
     * @param {!Object} credentials Credentials of the completed authentication.
     * @private
     */
    onAuthCompleted_: function(credentials) {
      if (credentials.useOffline) {
        this.email = credentials.email;
        chrome.send('authenticateUser',
                    [credentials.email, credentials.password]);
      } else if (credentials.authCode) {
        chrome.send('completeAuthentication',
                    [credentials.email,
                     credentials.password,
                     credentials.authCode]);
      } else {
        chrome.send('completeLogin',
                    [credentials.email, credentials.password]);
      }

      this.loading = true;
      // Now that we're in logged in state header should be hidden.
      Oobe.getInstance().headerHidden = true;
      // Clear any error messages that were shown before login.
      Oobe.clearErrors();
    },

    /**
     * Clears input fields and switches to input mode.
     * @param {boolean} takeFocus True to take focus.
     * @param {boolean} forceOnline Whether online sign-in should be forced.
     * If |forceOnline| is false previously used sign-in type will be used.
     */
    reset: function(takeFocus, forceOnline) {
      // Reload and show the sign-in UI if needed.
      if (takeFocus) {
        if (!forceOnline && this.isLocal) {
          // Show 'Cancel' button to allow user to return to the main screen
          // (e.g. this makes sense when connection is back).
          Oobe.getInstance().headerHidden = false;
          $('login-header-bar').signinUIState = SIGNIN_UI_STATE.GAIA_SIGNIN;
          // Do nothing, since offline version is reloaded after an error comes.
        } else {
          Oobe.showSigninUI();
        }
      }
    },

    /**
     * Reloads extension frame.
     */
    doReload: function() {
      this.error_ = 0;
      this.gaiaAuthHost_.reload();
      this.loading = true;
      this.startLoadingTimer_();
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      $('createAccount').innerHTML = loadTimeData.getStringF(
          'createAccount',
          '<a id="createAccountLink" class="signin-link" href="#">',
          '</a>');
      $('guestSignin').innerHTML = loadTimeData.getStringF(
          'guestSignin',
          '<a id="guestSigninLink" class="signin-link" href="#">',
          '</a>');
      $('createManagedUserLinkPlaceholder').innerHTML = loadTimeData.getStringF(
            'createLocallyManagedUser',
            '<a id="createManagedUserLink" class="signin-link" href="#">',
            '</a>');
      $('createAccountLink').onclick = function() {
        chrome.send('createAccount');
      };
      $('guestSigninLink').onclick = function() {
        chrome.send('launchIncognito');
      };
      $('createManagedUserLink').onclick = function() {
        chrome.send('showLocallyManagedUserCreationScreen');
      };
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} content Content to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      if (this.isLocal) {
        $('add-user-button').hidden = true;
        $('cancel-add-user-button').hidden = false;
        // Reload offline version of the sign-in extension, which will show
        // error itself.
        chrome.send('offlineLogin', [this.email]);
      } else if (!this.loading) {
        // We want to show bubble near "Email" field, but we can't calculate
        // it's position because it is located inside iframe. So we only
        // can hardcode some constants.
        /** @const */ var ERROR_BUBBLE_OFFSET = 84;
        /** @const */ var ERROR_BUBBLE_PADDING = 0;
        $('bubble').showContentForElement($('login-box'),
                                          cr.ui.Bubble.Attachment.LEFT,
                                          error,
                                          ERROR_BUBBLE_OFFSET,
                                          ERROR_BUBBLE_PADDING);
      } else {
        // Defer the bubble until the frame has been loaded.
        this.errorBubble_ = [loginAttempts, error];
      }
    },

    /**
     * Called when user canceled signin.
     */
    cancel: function() {
      if (!this.cancelAllowed_)
        return;
      $('pod-row').loadLastWallpaper();
      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
      Oobe.resetSigninUI(true);
    },

    /**
     * Handler for iframe's error notification coming from the outside.
     * For more info see C++ class 'SnifferObserver' which calls this method.
     * @param {number} error Error code.
     */
    onFrameError: function(error) {
      console.error('Gaia frame error = ' + error);
      if (error == NET_ERROR.ABORTED_BY_USER) {
        // Gaia frame was reloaded. Nothing to do here.
        return;
      }
      this.error_ = error;
      // Check current network state if currentScreen is a Gaia signin.
      var currentScreen = Oobe.getInstance().currentScreen;
      if (currentScreen.id == SCREEN_GAIA_SIGNIN)
        chrome.send('showGaiaFrameError', [error]);
    }
  };
});

