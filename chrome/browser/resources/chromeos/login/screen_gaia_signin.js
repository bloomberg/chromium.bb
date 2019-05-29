// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

'use strict';

(function() {

// GAIA animation guard timer. Started when GAIA page is loaded
// (Authenticator 'ready' event) and is intended to guard against edge cases
// when 'showView' message is not generated/received.
/** @const */ var GAIA_ANIMATION_GUARD_MILLISEC = 300;

// Maximum Gaia loading time in seconds.
/** @const */ var MAX_GAIA_LOADING_TIME_SEC = 60;

// The help topic regarding user not being in the whitelist.
/** @const */ var HELP_CANT_ACCESS_ACCOUNT = 188036;

// Amount of time the user has to be idle for before showing the online login
// page.
/** @const */ var IDLE_TIME_UNTIL_EXIT_OFFLINE_IN_MILLISECONDS = 180 * 1000;

// Approximate amount of time between checks to see if we should go to the
// online login page when we're in the offline login page and the device is
// online.
/** @const */ var IDLE_TIME_CHECK_FREQUENCY = 5 * 1000;

// Amount of time allowed for video based SAML logins, to prevent a site
// from keeping the camera on indefinitely.  This is a hard deadline and
// it will not be extended by user activity.
/** @const */ var VIDEO_LOGIN_TIMEOUT = 90 * 1000;

// Horizontal padding for the error bubble.
/** @const */ var BUBBLE_HORIZONTAL_PADDING = 65;

// Vertical padding for the error bubble.
/** @const */ var BUBBLE_VERTICAL_PADDING = -213;

/**
 * The modes this screen can be in.
 * @enum {integer}
 */
var ScreenMode = {
  DEFAULT: 0,            // Default GAIA login flow.
  OFFLINE: 1,            // GAIA offline login.
  SAML_INTERSTITIAL: 2,  // Interstitial page before SAML redirection.
  AD_AUTH: 3             // Offline Active Directory login flow.
};

Polymer({
  is: 'gaia-signin',

  behaviors: [LoginScreenBehavior, OobeDialogHostBehavior],

  EXTERNAL_API: [
    'loadAuthExtension',
    'doReload',
    'monitorOfflineIdle',
    'updateControlsState',
    'showWhitelistCheckFailedError',
    'invalidateAd',
  ],

  properties: {},

  /**
   * Saved authenticator load params.
   * @type {?string}
   * @private
   */
  authenticatorParams_: null,

  /**
   * Current mode of this screen.
   * @type {integer}
   * @private
   */
  screenMode_: ScreenMode.DEFAULT,

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
   * Timer id of a guard timer that is fired in case 'showView' message
   * is not received from GAIA.
   * @type {number}
   * @private
   */
  loadAnimationGuardTimer_: undefined,

  /**
   * Timer id of the video login timer.
   * @type {number}
   * @private
   */
  videoTimer_: undefined,

  /**
   * Whether we've processed 'showView' message - either from GAIA or from
   * guard timer.
   * @type {boolean}
   * @private
   */
  showViewProcessed_: false,

  /**
   * Whether we've processed 'authCompleted' message.
   * @type {boolean}
   * @private
   */
  authCompleted_: false,

  /**
   * Value contained in the last received 'backButton' event.
   * @type {boolean}
   * @private
   */
  lastBackMessageValue_: false,

  /**
   * Flag for tests that saml page was loaded.
   * @type {boolean}
   */
  samlInterstitialPageReady: false,

  /**
   * SAML password confirmation attempt count.
   * @type {number}
   */
  samlPasswordConfirmAttempt_: 0,

  /**
   * Do we currently have a setTimeout task running that tries to bring us
   * back to the online login page after the user has idled for awhile? If so,
   * then this id will be non-negative.
   */
  tryToGoToOnlineLoginPageCallbackId_: -1,

  /**
   * The most recent period of time that the user has interacted. This is
   * only updated when the offline page is active and the device is online.
   */
  mostRecentUserActivity_: Date.now(),

  /**
   * The UI component that hosts IdP pages.
   * @type {!cr.login.Authenticator|undefined}
   */
  authenticator_: undefined,

  /** @override */
  ready: function() {
    this.authenticator_ = new cr.login.Authenticator(this.$$('#signin-frame'));
    this.authenticator_.addEventListener('ready', this.onAuthReady_.bind(this));

    var that = this;
    var $that = this.$;
    [this.authenticator_, this.$['offline-gaia'], this.$['offline-ad-auth']]
        .forEach(function(frame) {
          // Ignore events from currently inactive frame.
          var frameFilter = function(callback) {
            return function(e) {
              var currentFrame = null;
              switch (that.screenMode_) {
                case ScreenMode.DEFAULT:
                case ScreenMode.SAML_INTERSTITIAL:
                  currentFrame = that.authenticator_;
                  break;
                case ScreenMode.OFFLINE:
                  currentFrame = $that['offline-gaia'];
                  break;
                case ScreenMode.AD_AUTH:
                  currentFrame = $that['offline-ad-auth'];
                  break;
              }
              if (frame === currentFrame)
                callback.call(that, e);
            };
          };

          frame.addEventListener(
              'authCompleted', frameFilter(that.onAuthCompletedMessage_));
          frame.addEventListener('backButton', frameFilter(that.onBackButton_));
          frame.addEventListener(
              'dialogShown', frameFilter(that.onDialogShown_));
          frame.addEventListener(
              'dialogHidden', frameFilter(that.onDialogHidden_));
          frame.addEventListener(
              'menuItemClicked', frameFilter(that.onMenuItemClicked_));
        });

    this.authenticator_.addEventListener(
        'showView', this.onShowView_.bind(this));
    this.authenticator_.confirmPasswordCallback =
        this.onAuthConfirmPassword_.bind(this);
    this.authenticator_.noPasswordCallback = this.onAuthNoPassword_.bind(this);
    this.authenticator_.insecureContentBlockedCallback =
        this.onInsecureContentBlocked_.bind(this);
    this.authenticator_.missingGaiaInfoCallback =
        this.missingGaiaInfo_.bind(this);
    this.authenticator_.samlApiUsedCallback = this.samlApiUsed_.bind(this);
    this.authenticator_.getIsSamlUserPasswordlessCallback =
        this.getIsSamlUserPasswordless_.bind(this);
    this.authenticator_.addEventListener(
        'authDomainChange', this.onAuthDomainChange_.bind(this));
    this.authenticator_.addEventListener(
        'authFlowChange', this.onAuthFlowChange_.bind(this));
    this.authenticator_.addEventListener(
        'videoEnabledChange', this.onVideoEnabledChange_.bind(this));

    this.authenticator_.addEventListener(
        'loadAbort', this.onLoadAbortMessage_.bind(this));
    this.authenticator_.addEventListener(
        'identifierEntered', this.onIdentifierEnteredMessage_.bind(this));

    this.$['signin-back-button'].addEventListener(
        'tap', this.onBackButtonClicked_.bind(this));
    this.$['offline-gaia'].addEventListener(
        'offline-gaia-cancel', this.cancel.bind(this));

    this.$['gaia-whitelist-error'].addEventListener('buttonclick', function() {
      this.showWhitelistCheckFailedError(false);
    }.bind(this));

    this.$['gaia-whitelist-error'].addEventListener('linkclick', function() {
      chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
    });

    // Register handlers for the saml interstitial page events.
    this.$['saml-interstitial'].addEventListener(
        'samlPageNextClicked', function() {
          this.screenMode = ScreenMode.DEFAULT;
          this.loadAuthenticator_(true /* doSamlRedirect */);
        }.bind(this));
    this.$['saml-interstitial'].addEventListener(
        'samlPageChangeAccountClicked', function() {
          // The user requests to change the account. We must clear the email
          // field of the auth params.
          this.authenticatorParams_.email = '';
          this.screenMode = ScreenMode.DEFAULT;
          this.loadAuthenticator_(false /* doSamlRedirect */);
        }.bind(this));

    this.$['offline-ad-auth'].addEventListener('cancel', function() {
      this.cancel();
    }.bind(this));

    this.initializeLoginScreen('GaiaSigninScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Whether the dialog could be closed.
   * This is being checked in cancel() when user clicks on signin-back-button
   * (normal gaia flow) or the "Back" button in other authentication frames.
   * @type {boolean}
   */
  get closable() {
    return Oobe.getInstance().hasUserPods || this.isOffline();
  },

  /**
   * Returns true if the screen is at the beginning of flow (i.e. the email
   * page).
   * @type {boolean}
   */
  isAtTheBeginning: function() {
    return !this.canGoBack_() && !this.isSAML() &&
        !this.classList.contains('whitelist-error') && !this.authCompleted_;
  },

  /**
   * Updates visibility of UI control buttons.
   */
  updateControlsState: function() {
    let showGuestInOobe = !this.closable && this.isAtTheBeginning();
    chrome.send('showGuestInOobe', [showGuestInOobe]);
  },

  /**
   * Returns whether it's possible to rewind the sign-in frame one step back (as
   * opposed to cancelling the sign-in flow).
   * @type {boolean}
   * @private
   */
  canGoBack_: function() {
    var isWhitelistError = this.classList.contains('whitelist-error');
    return this.lastBackMessageValue_ && !isWhitelistError &&
        !this.authCompleted_ && !this.loading && !this.isSAML();
  },

  /**
   * Handles clicks on "Back" button.
   */
  onBackButtonClicked_: function() {
    if (!this.canGoBack_()) {
      this.cancel();
    } else {
      this.getSigninFrame_().back();
    }
  },

  /**
   * Loads the authenticator and updates the UI to reflect the loading state.
   * @param {boolean} doSamlRedirect If the authenticator should do
   *     authentication by automatic redirection to the SAML-based enrollment
   *     enterprise domain IdP.
   */
  loadAuthenticator_: function(doSamlRedirect) {
    this.loading = true;
    this.startLoadingTimer_();

    this.authenticatorParams_.doSamlRedirect = doSamlRedirect;
    this.authenticator_.load(
        cr.login.Authenticator.AuthMode.DEFAULT, this.authenticatorParams_);
  },

  /**
   * Returns true if offline version of Gaia is used.
   * @type {boolean}
   */
  isOffline: function() {
    return this.screenMode_ == ScreenMode.OFFLINE;
  },

  /**
   * Sets the current screen mode and updates the visible elements
   * accordingly.
   * @param {integer} value The screen mode.
   */
  set screenMode(value) {
    this.screenMode_ = value;
    switch (this.screenMode_) {
      case ScreenMode.DEFAULT:
        this.$['signin-frame-dialog'].hidden = false;
        this.$$('#signin-frame').hidden = false;
        this.$['offline-gaia'].hidden = true;
        this.$['saml-interstitial'].hidden = true;
        this.$['offline-ad-auth'].hidden = true;
        break;
      case ScreenMode.OFFLINE:
        this.$['signin-frame-dialog'].hidden = true;
        this.$$('#signin-frame').hidden = true;
        this.$['offline-gaia'].hidden = false;
        this.$['saml-interstitial'].hidden = true;
        this.$['offline-ad-auth'].hidden = true;
        break;
      case ScreenMode.AD_AUTH:
        this.$['signin-frame-dialog'].hidden = true;
        this.$$('#signin-frame').hidden = true;
        this.$['offline-gaia'].hidden = true;
        this.$['saml-interstitial'].hidden = true;
        this.$['offline-ad-auth'].hidden = false;
        break;
      case ScreenMode.SAML_INTERSTITIAL:
        this.$['signin-frame-dialog'].hidden = true;
        this.$$('#signin-frame').hidden = true;
        this.$['offline-gaia'].hidden = true;
        this.$['saml-interstitial'].hidden = false;
        this.$['offline-ad-auth'].hidden = true;
        break;
    }
    this.updateSigninFrameContainers_();
    chrome.send('updateOfflineLogin', [this.isOffline()]);
    this.updateControlsState();
  },

  /**
   * This enables or disables trying to go back to the online login page
   * after the user is idle for a few minutes, assuming that we're currently
   * in the offline one. This is only applicable when the offline page is
   * currently active. It is intended that when the device goes online, this
   * gets called with true; when it goes offline, this gets called with
   * false.
   */
  monitorOfflineIdle: function(shouldMonitor) {
    var ACTIVITY_EVENTS = ['click', 'mousemove', 'keypress'];
    var self = this;

    // updateActivityTime_ is used as a callback for addEventListener, so we
    // need the exact reference for removeEventListener. Because the callback
    // needs to access the |this| as scoped inside of this function, we create
    // a closure that uses the appropriate |this|.
    //
    // Unfourtantely, we cannot define this function inside of the JSON object
    // as then we have to no way to create to capture the correct |this|
    // reference. We define it here instead.
    if (!self.updateActivityTime_) {
      self.updateActivityTime_ = function() {
        self.mostRecentUserActivity_ = Date.now();
      };
    }

    // Begin monitoring.
    if (shouldMonitor) {
      // If we're not using the offline login page or we're already
      // monitoring, then we don't need to do anything.
      if (!self.isOffline() ||
          self.tryToGoToOnlineLoginPageCallbackId_ !== -1) {
        return;
      }

      self.mostRecentUserActivity_ = Date.now();
      ACTIVITY_EVENTS.forEach(function(event) {
        document.addEventListener(event, self.updateActivityTime_);
      });

      self.tryToGoToOnlineLoginPageCallbackId_ = setInterval(function() {
        // If we're not in the offline page or the signin page, then we want
        // to terminate monitoring.
        if (!self.isOffline() ||
            Oobe.getInstance().currentScreen.id != 'gaia-signin') {
          self.monitorOfflineIdle(false);
          return;
        }

        var idleDuration = Date.now() - self.mostRecentUserActivity_;
        if (idleDuration > IDLE_TIME_UNTIL_EXIT_OFFLINE_IN_MILLISECONDS) {
          self.monitorOfflineIdle(false);
          Oobe.resetSigninUI(true);
        }
      }, IDLE_TIME_CHECK_FREQUENCY);
    }

    // Stop monitoring.
    else {
      // We're not monitoring, so we don't need to do anything.
      if (self.tryToGoToOnlineLoginPageCallbackId_ === -1)
        return;

      ACTIVITY_EVENTS.forEach(function(event) {
        document.removeEventListener(event, self.updateActivityTime_);
      });
      clearInterval(self.tryToGoToOnlineLoginPageCallbackId_);
      self.tryToGoToOnlineLoginPageCallbackId_ = -1;
    }
  },

  /**
   * Shows/hides loading UI.
   * @param {boolean} show True to show loading UI.
   * @private
   */
  showLoadingUI_: function(show) {
    this.$['gaia-loading'].hidden = !show;

    // Only set hidden for offline-gaia or saml-interstitial and not set it on
    // the 'sign-frame' webview element. Setting it on webview not only hides
    // but also affects its loading events.
    if (this.screenMode_ != ScreenMode.DEFAULT)
      this.getSigninFrame_().hidden = show;
    this.getSigninFrame_().classList.toggle('show', !show);
    this.classList.toggle('loading', show);
    this.updateControlsState();
  },

  /**
   * Handler for Gaia loading timeout.
   * @private
   */
  onLoadingTimeOut_: function() {
    if (Oobe.getInstance().currentScreen.id != 'gaia-signin')
      return;
    this.loadingTimer_ = undefined;
    chrome.send('showLoadingTimeoutError');
  },

  /**
   * Clears loading timer.
   * @private
   */
  clearLoadingTimer_: function() {
    if (this.loadingTimer_) {
      clearTimeout(this.loadingTimer_);
      this.loadingTimer_ = undefined;
    }
  },

  /**
   * Sets up loading timer.
   * @private
   */
  startLoadingTimer_: function() {
    this.clearLoadingTimer_();
    this.loadingTimer_ = setTimeout(
        this.onLoadingTimeOut_.bind(this), MAX_GAIA_LOADING_TIME_SEC * 1000);
  },

  /**
   * Handler for GAIA animation guard timer.
   * @private
   */
  onLoadAnimationGuardTimer_: function() {
    this.loadAnimationGuardTimer_ = undefined;
    this.onShowView_();
  },

  /**
   * Clears GAIA animation guard timer.
   * @private
   */
  clearLoadAnimationGuardTimer_: function() {
    if (this.loadAnimationGuardTimer_) {
      clearTimeout(this.loadAnimationGuardTimer_);
      this.loadAnimationGuardTimer_ = undefined;
    }
  },

  /**
   * Sets up GAIA animation guard timer.
   * @private
   */
  startLoadAnimationGuardTimer_: function() {
    this.clearLoadAnimationGuardTimer_();
    this.loadAnimationGuardTimer_ = setTimeout(
        this.onLoadAnimationGuardTimer_.bind(this),
        GAIA_ANIMATION_GUARD_MILLISEC);
  },

  /**
   * Whether Gaia is loading.
   * @type {boolean}
   */
  get loading() {
    return !this.$['gaia-loading'].hidden;
  },
  set loading(loading) {
    if (loading == this.loading)
      return;

    this.showLoadingUI_(loading);
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });

    this.screenMode = ScreenMode.DEFAULT;
    this.loading = true;
    chrome.send('loginUIStateChanged', ['gaia-signin', true]);
    Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.GAIA_SIGNIN);

    // Ensure that GAIA signin (or loading UI) is actually visible.
    window.requestAnimationFrame(function() {
      chrome.send('loginVisible', ['gaia-loading']);
    });

    // Re-enable navigation in case it was disabled before refresh.
    this.navigationDisabled_ = false;

    this.lastBackMessageValue_ = false;
    this.updateControlsState();

    this.$['offline-ad-auth'].onBeforeShow();
    this.$['signin-frame-dialog'].onBeforeShow();
  },

  set navigationDisabled_(value) {
    this.$['gaia-screen-buttons-overlay'].hidden = !value;
    this.$['signin-back-button'].disabled = value;
  },

  getSigninFrame_: function() {
    switch (this.screenMode_) {
      case ScreenMode.DEFAULT:
        return this.$$('#signin-frame');
      case ScreenMode.OFFLINE:
        return this.$['offline-gaia'];
      case ScreenMode.AD_AUTH:
        return this.$['offline-ad-auth'];
      case ScreenMode.SAML_INTERSTITIAL:
        return this.$['saml-interstitial'];
    }
  },

  focusSigninFrame: function() {
    this.getSigninFrame_().focus();
  },

  onAfterShow: function() {
    if (!this.loading)
      this.focusSigninFrame();
  },

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  onBeforeHide: function() {
    chrome.send('loginUIStateChanged', ['gaia-signin', false]);
    Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.HIDDEN);
    this.$['offline-gaia'].switchToEmailCard(false /* animated */);
  },

  /**
   * Copies attributes between nodes.
   * @param {!Object} fromNode source to copy attributes from
   * @param {!Object} toNode target to copy attributes to
   * @param {!Set<string>} skipAttributes specifies attributes to be skipped
   * @private
   */
  copyAttributes_: function(fromNode, toNode, skipAttributes) {
    for (var i = 0; i < fromNode.attributes.length; ++i) {
      var attribute = fromNode.attributes[i];
      if (!skipAttributes.has(attribute.nodeName))
        toNode.setAttribute(attribute.nodeName, attribute.nodeValue);
    }
  },

  /**
   * Changes the 'partition' attribute of the sign-in frame. If the sign-in
   * frame has already navigated, this function re-creates it.
   * @param {string} newWebviewPartitionName the new partition
   * @private
   */
  setSigninFramePartition_: function(newWebviewPartitionName) {
    var signinFrame = this.$$('#signin-frame');

    if (!signinFrame.src) {
      // We have not navigated anywhere yet. Note that a webview's src
      // attribute does not allow a change back to "".
      signinFrame.partition = newWebviewPartitionName;
    } else if (signinFrame.partition != newWebviewPartitionName) {
      // The webview has already navigated. We have to re-create it.
      var signinFrameParent = signinFrame.parentElement;

      // Copy all attributes except for partition and src from the previous
      // webview. Use the specified |newWebviewPartitionName|.
      var newSigninFrame = document.createElement('webview');
      this.copyAttributes_(
          signinFrame, newSigninFrame, new Set(['src', 'partition']));
      newSigninFrame.partition = newWebviewPartitionName;

      signinFrameParent.replaceChild(newSigninFrame, signinFrame);

      // Make sure the authenticator uses the new webview from now on.
      this.authenticator_.rebindWebview(newSigninFrame);
    }
  },

  /**
   * Loads the authentication extension into the iframe.
   * @param {Object} data Extension parameters bag.
   * @private
   */
  loadAuthExtension: function(data) {
    // Redirect the webview to the blank page in order to stop the SAML IdP
    // page from working in a background (see crbug.com/613245).
    if (this.screenMode_ == ScreenMode.DEFAULT &&
        data.screenMode != ScreenMode.DEFAULT) {
      this.authenticator_.resetWebview();
    }

    this.setSigninFramePartition_(data.webviewPartitionName);

    // This triggers updateSigninFrameContainers_()
    this.screenMode = data.screenMode;
    this.email = '';
    this.authCompleted_ = false;
    this.lastBackMessageValue_ = false;

    // Reset SAML
    this.$['saml-notice-container'].hidden = true;
    this.samlPasswordConfirmAttempt_ = 0;

    this.updateSigninFrameContainers_();

    var params = {};
    for (var i in cr.login.Authenticator.SUPPORTED_PARAMS) {
      var name = cr.login.Authenticator.SUPPORTED_PARAMS[i];
      if (data[name])
        params[name] = data[name];
    }

    params.doSamlRedirect = (this.screenMode_ == ScreenMode.SAML_INTERSTITIAL);
    params.menuGuestMode = data.guestSignin;
    params.menuKeyboardOptions = false;
    params.menuEnterpriseEnrollment =
        !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.isFirstUser = !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.obfuscatedOwnerId = data.obfuscatedOwnerId;

    this.authenticatorParams_ = params;

    switch (this.screenMode_) {
      case ScreenMode.DEFAULT:
        this.loadAuthenticator_(false /* doSamlRedirect */);
        break;

      case ScreenMode.OFFLINE:
        this.loadOffline(params);
        break;

      case ScreenMode.AD_AUTH:
        this.loadAdAuth(params);
        break;

      case ScreenMode.SAML_INTERSTITIAL:
        this.$['saml-interstitial'].domain = data.enterpriseDisplayDomain;
        if (this.loading)
          this.loading = false;
        // This event is for the browser tests.
        this.samlInterstitialPageReady = true;
        this.$['saml-interstitial'].fire('samlInterstitialPageReady');
        break;
    }
    this.updateControlsState();
    chrome.send('authExtensionLoaded');
  },

  /**
   * Displays correct screen container for given mode and APi version.
   */
  updateSigninFrameContainers_: function() {
    let oldState = this.classList.contains('v2');
    this.classList.toggle('v2', false);
    if (this.screenMode_ == ScreenMode.DEFAULT ||
        this.screenMode_ == ScreenMode.OFFLINE ||
        this.screenMode_ == ScreenMode.AD_AUTH) {
      this.classList.toggle('v2', true);
    }
    if (Oobe.getInstance().currentScreen.id != 'gaia-signin')
      return;
    // Switching between signin-frame-dialog and gaia-step-contents
    // updates screen size.
    if (oldState != this.classList.contains('v2'))
      Oobe.getInstance().updateScreenSize(this);
  },

  /**
   * Whether the current auth flow is SAML.
   */
  isSAML: function() {
    return this.authenticator_.authFlow == cr.login.Authenticator.AuthFlow.SAML;
  },

  /**
   * Helper function to update the title bar.
   */
  updateSamlNotice_: function() {
    if (this.authenticator_.videoEnabled) {
      this.$['saml-notice-message'].textContent = loadTimeData.getStringF(
          'samlNoticeWithVideo', this.authenticator_.authDomain);
      this.$['saml-notice-recording-indicator'].hidden = false;
      this.$['saml-notice-container'].style.justifyContent = 'flex-start';
    } else {
      this.$['saml-notice-message'].textContent =
          loadTimeData.getStringF('samlNotice', this.authenticator_.authDomain);
      this.$['saml-notice-recording-indicator'].hidden = true;
      this.$['saml-notice-container'].style.justifyContent = 'center';
    }
  },

  /**
   * Clean up from a video-enabled SAML flow.
   */
  clearVideoTimer_: function() {
    if (this.videoTimer_ !== undefined) {
      clearTimeout(this.videoTimer_);
      this.videoTimer_ = undefined;
    }
  },

  /**
   * Invoked when the authDomain property is changed on the authenticator.
   */
  onAuthDomainChange_: function() {
    this.updateSamlNotice_();
  },

  /**
   * Invoked when the videoEnabled property is changed on the authenticator.
   */
  onVideoEnabledChange_: function() {
    this.updateSamlNotice_();
    if (this.authenticator_.videoEnabled && this.videoTimer_ === undefined) {
      this.videoTimer_ =
          setTimeout(this.cancel.bind(this), VIDEO_LOGIN_TIMEOUT);
    } else {
      this.clearVideoTimer_();
    }
  },

  /**
   * Invoked when the authFlow property is changed on the authenticator.
   */
  onAuthFlowChange_: function() {
    var isSAML = this.isSAML();

    this.$['saml-notice-container'].hidden = !isSAML;
    this.classList.toggle('saml', isSAML);

    if (Oobe.getInstance().currentScreen.id == 'gaia-signin') {
      Oobe.getInstance().updateScreenSize(this);
    }

    this.updateControlsState();
  },

  /**
   * Invoked when the authenticator emits 'ready' event or when another
   * authentication frame is completely loaded.
   * @private
   */
  onAuthReady_: function() {
    this.showViewProcessed_ = false;
    this.startLoadAnimationGuardTimer_();
    this.clearLoadingTimer_();
    this.loading = false;

    if (!this.$['offline-gaia'].hidden)
      this.$['offline-gaia'].focus();
  },

  /**
   * Invoked when a fra,e emits 'dialogShown' event.
   * @private
   */
  onDialogShown_: function() {
    this.navigationDisabled_ = true;
  },

  /**
   * Invoked when a frame emits 'dialogHidden' event.
   * @private
   */
  onDialogHidden_: function() {
    this.navigationDisabled_ = false;
  },

  /**
   * Invoked when user activates menu item.
   * @private
   */
  onMenuItemClicked_: function(e) {
    if (e.detail == 'gm') {
      Oobe.disableSigninUI();
      chrome.send('launchIncognito');
    } else if (e.detail == 'ee') {
      cr.ui.Oobe.handleAccelerator(ACCELERATOR_ENROLLMENT);
    }
  },

  /**
   * Invoked when the authenticator requests whether the specified user is a
   * user without a password (neither a manually entered one nor one provided
   * via Credentials Passing API).
   * @param {string} email
   * @param {string} gaiaId
   * @param {function(boolean)} callback
   * @private
   */
  getIsSamlUserPasswordless_: function(email, gaiaId, callback) {
    cr.sendWithPromise('getIsSamlUserPasswordless', email, gaiaId)
        .then(callback);
  },

  /**
   * Invoked when a frame emits 'backButton' event.
   * @private
   */
  onBackButton_: function(e) {
    this.getSigninFrame_().focus();
    this.lastBackMessageValue_ = !!e.detail;
    this.updateControlsState();
  },

  /**
   * Invoked when the authenticator emits 'showView' event or when corresponding
   * guard time fires.
   * @private
   */
  onShowView_: function(e) {
    if (this.showViewProcessed_)
      return;

    this.showViewProcessed_ = true;
    this.clearLoadAnimationGuardTimer_();
    this.getSigninFrame_().classList.toggle('show', true);
    this.onLoginUIVisible_();
  },

  /**
   * Called when UI is shown.
   * @private
   */
  onLoginUIVisible_: function() {
    // Show deferred error bubble.
    if (this.errorBubble_) {
      this.showErrorBubble(this.errorBubble_[0], this.errorBubble_[1]);
      this.errorBubble_ = undefined;
    }

    chrome.send('loginWebuiReady');
    chrome.send('loginVisible', ['gaia-signin']);
  },

  /**
   * Invoked when the user has successfully authenticated via SAML, the
   * principals API was not used and the authenticator needs the user to confirm
   * the scraped password.
   * @param {string} email The authenticated user's e-mail.
   * @param {number} passwordCount The number of passwords that were scraped.
   * @private
   */
  onAuthConfirmPassword_: function(email, passwordCount) {
    this.loading = true;

    if (this.samlPasswordConfirmAttempt_ == 0)
      chrome.send('scrapedPasswordCount', [passwordCount]);

    if (this.samlPasswordConfirmAttempt_ < 2) {
      login.ConfirmPasswordScreen.show(
          email, false /* manual password entry */,
          this.samlPasswordConfirmAttempt_,
          this.onConfirmPasswordCollected_.bind(this));
    } else {
      chrome.send('scrapedPasswordVerificationFailed');
      this.showFatalAuthError(
          loadTimeData.getString('fatalErrorMessageVerificationFailed'),
          loadTimeData.getString('fatalErrorTryAgainButton'));
    }
  },

  /**
   * Invoked when the confirm password screen is dismissed.
   * @private
   */
  onConfirmPasswordCollected_: function(password) {
    this.samlPasswordConfirmAttempt_++;
    this.authenticator_.verifyConfirmedPassword(password);

    // Shows signin UI again without changing states.
    Oobe.showScreen({id: SCREEN_GAIA_SIGNIN});
  },

  /**
   * Inovked when the user has successfully authenticated via SAML, the
   * principals API was not used and no passwords could be scraped.
   * The user will be asked to pick a manual password for the device.
   * @param {string} email The authenticated user's e-mail.
   */
  onAuthNoPassword_: function(email) {
    chrome.send('scrapedPasswordCount', [0]);
    login.ConfirmPasswordScreen.show(
        email, true /* manual password entry */,
        this.samlPasswordConfirmAttempt_,
        this.onManualPasswordCollected_.bind(this));
  },

  /**
   * Invoked when the dialog where the user enters a manual password for the
   * device, when password scraping fails.
   * @param {string} password The password the user entered. Not necessarily
   *     the same as their SAML password.
   */
  onManualPasswordCollected_: function(password) {
    this.authenticator_.completeAuthWithManualPassword(password);
  },

  /**
   * Invoked when the authentication flow had to be aborted because content
   * served over an unencrypted connection was detected. Shows a fatal error.
   * This method is only called on Chrome OS, where the entire authentication
   * flow is required to be encrypted.
   * @param {string} url The URL that was blocked.
   */
  onInsecureContentBlocked_: function(url) {
    this.showFatalAuthError(
        loadTimeData.getStringF('fatalErrorMessageInsecureURL', url),
        loadTimeData.getString('fatalErrorDoneButton'));
  },

  /**
   * Shows the fatal auth error.
   * @param {string} message The error message to show.
   * @param {string} buttonLabel The label to display on dismiss button.
   */
  showFatalAuthError: function(message, buttonLabel) {
    login.FatalErrorScreen.show(message, buttonLabel, Oobe.showSigninUI);
  },

  /**
   * Show fatal auth error when information is missing from GAIA.
   */
  missingGaiaInfo_: function() {
    this.showFatalAuthError(
        loadTimeData.getString('fatalErrorMessageNoAccountDetails'),
        loadTimeData.getString('fatalErrorTryAgainButton'));
  },

  /**
   * Record that SAML API was used during sign-in.
   */
  samlApiUsed_: function() {
    chrome.send('usingSAMLAPI');
  },

  /**
   * Invoked when auth is completed successfully.
   * @param {!Object} credentials Credentials of the completed authentication.
   * @private
   */
  onAuthCompleted_: function(credentials) {
    if (this.screenMode_ == ScreenMode.AD_AUTH) {
      this.email = credentials.username;
      chrome.send(
          'completeAdAuthentication',
          [credentials.username, credentials.password]);
    } else if (credentials.useOffline) {
      this.email = credentials.email;
      chrome.send(
          'completeOfflineAuthentication',
          [credentials.email, credentials.password]);
    } else {
      chrome.send('completeAuthentication', [
        credentials.gaiaId, credentials.email, credentials.password,
        credentials.usingSAML, credentials.services,
        credentials.passwordAttributes
      ]);
    }

    this.loading = true;
    // Hide the back button and the border line as they are not useful
    // when the loading screen is shown.
    this.$['signin-back-button'].hidden = true;
    this.$['signin-frame-dialog'].setAttribute('hide-shadow', true);

    // Clear any error messages that were shown before login.
    Oobe.clearErrors();

    this.clearVideoTimer_();
    this.authCompleted_ = true;
    this.updateControlsState();
  },

  /**
   * Invoked when onAuthCompleted message received.
   * @param {!Object} e Payload of the received HTML5 message.
   * @private
   */
  onAuthCompletedMessage_: function(e) {
    this.onAuthCompleted_(e.detail);
  },

  /**
   * Invoked when onLoadAbort message received.
   * @param {!Object} e Payload of the received HTML5 message.
   * @private
   */
  onLoadAbortMessage_: function(e) {
    this.onWebviewError(e.detail);
  },

  /**
   * Invoked when identifierEntered message received.
   * @param {!Object} e Payload of the received HTML5 message.
   * @private
   */
  onIdentifierEnteredMessage_: function(e) {
    this.onIdentifierEntered(e.detail);
  },

  /**
   * Clears input fields and switches to input mode.
   * @param {boolean} takeFocus True to take focus.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  reset: function(takeFocus, forceOnline) {
    // Reload and show the sign-in UI if needed.
    this.authenticator_.resetStates();
    if (takeFocus) {
      if (!forceOnline && this.isOffline()) {
        Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.GAIA_SIGNIN);
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
    if (this.screenMode_ != ScreenMode.DEFAULT)
      return;
    this.authenticator_.reload();
    this.loading = true;
    this.startLoadingTimer_();
    this.lastBackMessageValue_ = false;
    this.authCompleted_ = false;
    this.updateControlsState();
  },

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {HTMLElement} content Content to show in bubble.
   */
  showErrorBubble: function(loginAttempts, error) {
    if (this.isOffline()) {
      // Reload offline version of the sign-in extension, which will show
      // error itself.
      chrome.send('offlineLogin', [this.email]);
    } else if (!this.loading) {
      $('bubble').showContentForElement(
          this, cr.ui.Bubble.Attachment.BOTTOM, error,
          BUBBLE_HORIZONTAL_PADDING, BUBBLE_VERTICAL_PADDING);
    } else {
      // Defer the bubble until the frame has been loaded.
      this.errorBubble_ = [loginAttempts, error];
    }
  },

  /**
   * Called when user canceled signin.
   */
  cancel: function() {
    this.clearVideoTimer_();

    const isWhitelistError = this.classList.contains('whitelist-error');
    // TODO(crbug.com/470893): Figure out whether/which of these exit conditions
    // are useful.
    if (this.screenMode_ == ScreenMode.SAML_INTERSTITIAL || isWhitelistError ||
        this.authCompleted_) {
      return;
    }

    if (this.screenMode_ == ScreenMode.AD_AUTH)
      chrome.send('cancelAdAuthentication');

    if (this.closable)
      Oobe.showUserPods();
    else
      Oobe.resetSigninUI(true);
  },

  /**
   * Handler for webview error handling.
   * @param {!Object} data Additional information about error event like:
   * {string} error Error code such as "ERR_INTERNET_DISCONNECTED".
   * {string} url The URL that failed to load.
   */
  onWebviewError: function(data) {
    chrome.send('webviewLoadAborted', [data.error]);
  },

  /**
   * Handler for identifierEntered event.
   * @param {!Object} data The identifier entered by user:
   * {string} accountIdentifier User identifier.
   */
  onIdentifierEntered: function(data) {
    chrome.send('identifierEntered', [data.accountIdentifier]);
  },

  /**
   * Sets enterprise info strings for offline gaia.
   * Also sets callback and sends message whether we already have email and
   * should switch to the password screen with error.
   */
  loadOffline: function(params) {
    this.loading = true;
    this.startLoadingTimer_();
    var offlineLogin = this.$['offline-gaia'];
    if ('enterpriseDisplayDomain' in params)
      offlineLogin.domain = params['enterpriseDisplayDomain'];
    if ('emailDomain' in params)
      offlineLogin.emailDomain = '@' + params['emailDomain'];
    offlineLogin.setEmail(params.email);
    this.onAuthReady_();
  },

  loadAdAuth: function(params) {
    this.loading = true;
    this.startLoadingTimer_();
    var adAuthUI = this.getSigninFrame_();
    adAuthUI.realm = params['realm'];

    if ('emailDomain' in params)
      adAuthUI.userRealm = '@' + params['emailDomain'];

    adAuthUI.userName = params['email'];
    adAuthUI.focus();
    this.onAuthReady_();
  },

  /**
   * Show/Hide error when user is not in whitelist. When UI is hidden
   * GAIA is reloaded.
   * @param {boolean} show Show/hide error UI.
   * @param {!Object} opt_data Optional additional information.
   */
  showWhitelistCheckFailedError: function(show, opt_data) {
    if (show) {
      var isManaged = opt_data && opt_data.enterpriseManaged;
      this.$['gaia-whitelist-error'].textContent = loadTimeData.getValue(
          isManaged ? 'whitelistErrorEnterprise' : 'whitelistErrorConsumer');
      // To make animations correct, we need to make sure Gaia is completely
      // reloaded. Otherwise ChromeOS overlays hide and Gaia page is shown
      // somewhere in the middle of animations.
      if (this.screenMode_ == ScreenMode.DEFAULT)
        this.authenticator_.resetWebview();
    }

    this.classList.toggle('whitelist-error', show);
    this.loading = !show;

    if (show)
      this.$['gaia-whitelist-error'].submitButton.focus();
    else
      Oobe.showSigninUI();

    this.updateControlsState();
  },

  invalidateAd: function(username, errorState) {
    if (this.screenMode_ != ScreenMode.AD_AUTH)
      return;
    var adAuthUI = this.getSigninFrame_();
    adAuthUI.userName = username;
    adAuthUI.errorState = errorState;
    this.authCompleted_ = false;
    this.loading = false;
  },

});
})();
