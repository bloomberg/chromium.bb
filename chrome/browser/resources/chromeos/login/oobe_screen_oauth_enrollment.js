// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('OAuthEnrollmentScreen', 'oauth-enrollment', function() {
  /* Code which is embedded inside of the webview. See below for details.
  /** @const */ var INJECTED_WEBVIEW_SCRIPT = String.raw`
                      (function() {
                         <include src="../keyboard/keyboard_utils.js">
                         keyboard.initializeKeyboardFlow();
                       })();`;

  /** @const */ var STEP_SIGNIN = 'signin';
  /** @const */ var STEP_WORKING = 'working';
  /** @const */ var STEP_ATTRIBUTE_PROMPT = 'attribute-prompt';
  /** @const */ var STEP_ERROR = 'error';
  /** @const */ var STEP_SUCCESS = 'success';

  /* TODO(dzhioev): define this step on C++ side.
  /** @const */ var STEP_ATTRIBUTE_PROMPT_ERROR = 'attribute-prompt-error';

  /** @const */ var HELP_TOPIC_ENROLLMENT = 4631259;

  return {
    EXTERNAL_API: [
      'showStep',
      'showError',
      'doReload',
      'showAttributePromptStep',
    ],

    /**
     * Authenticator object that wraps GAIA webview.
     */
    authenticator_: null,

    /**
     * The current step. This is the last value passed to showStep().
     */
    currentStep_: null,

    /**
     * We block esc, back button and cancel button until gaia is loaded to
     * prevent multiple cancel events.
     */
    isCancelDisabled_: null,

    get isCancelDisabled() { return this.isCancelDisabled_ },
    set isCancelDisabled(disabled) {
      if (disabled == this.isCancelDisabled)
        return;
      this.isCancelDisabled_ = disabled;
    },

    /** @override */
    decorate: function() {
      var webview = document.createElement('webview');
      webview.id = webview.name = 'oauth-enroll-auth-view';
      $('oauth-enroll-auth-view-container').appendChild(webview);
      this.authenticator_ = new cr.login.Authenticator(webview);

      this.authenticator_.addEventListener('ready',
          (function() {
            if (this.currentStep_ != STEP_SIGNIN)
              return;
            this.isCancelDisabled = false;
            chrome.send('frameLoadingCompleted', [0]);
          }).bind(this));

      this.authenticator_.addEventListener('authCompleted',
          (function(e) {
            var detail = e.detail;
            if (!detail.email || !detail.authCode) {
              this.showError(
                  loadTimeData.getString('fatalEnrollmentError'),
                  false);
              return;
            }
            chrome.send('oauthEnrollCompleteLogin', [detail.email,
                                                     detail.authCode]);
          }).bind(this));

      this.authenticator_.addEventListener('authFlowChange',
          (function(e) {
            var isSAML = this.authenticator_.authFlow ==
                             cr.login.Authenticator.AuthFlow.SAML;
            if (isSAML) {
              $('oauth-saml-notice-message').textContent =
                  loadTimeData.getStringF('samlNotice',
                                          this.authenticator_.authDomain);
            }
            this.classList.toggle('saml', isSAML);
            if (Oobe.getInstance().currentScreen == this)
              Oobe.getInstance().updateScreenSize(this);
          }).bind(this));

      this.authenticator_.addEventListener('backButton',
          (function(e) {
            $('oauth-enroll-back-button').hidden = !e.detail;
            $('oauth-enroll-auth-view').focus();
            $('login-header-bar').updateUI_();
          }).bind(this));

      this.authenticator_.insecureContentBlockedCallback =
          (function(url) {
            this.showError(
                loadTimeData.getStringF('insecureURLEnrollmentError', url),
                false);
          }).bind(this);

      this.authenticator_.missingGaiaInfoCallback =
          (function() {
            this.showError(
                loadTimeData.getString('fatalEnrollmentError'),
                false);
          }).bind(this);

      $('oauth-enroll-error-card').addEventListener('buttonclick',
                                                    this.doRetry_.bind(this));
      function doneCallback() {
        chrome.send('oauthEnrollClose', ['done']);
      };

      $('oauth-enroll-attribute-prompt-error-card').addEventListener(
          'buttonclick', doneCallback);
      $('oauth-enroll-success-card').addEventListener(
          'buttonclick', doneCallback);

      $('oauth-enroll-cancel-button').addEventListener('click',
                                                       this.cancel.bind(this));
      $('oauth-enroll-refresh-button').addEventListener('click',
                                                        this.cancel.bind(this));

      $('oauth-enroll-back-button').addEventListener('click',
          (function(e) {
            $('oauth-enroll-back-button').hidden = true;
            $('oauth-enroll-auth-view').back();
            e.preventDefault();
          }).bind(this));

      $('oauth-enroll-attribute-prompt-card').addEventListener('submit',
          this.onAttributesSubmitted.bind(this));

      $('oauth-enroll-learn-more-link').addEventListener('click',
          function(event) {
            chrome.send('oauthEnrollOnLearnMore');
          });

      $('oauth-enroll-skip-button').addEventListener('click',
          this.onSkipButtonClicked.bind(this));
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('oauthEnrollScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var ownerDocument = this.ownerDocument;

      function makeButton(id, classes, label, handler) {
        var button = ownerDocument.createElement('button');
        button.id = id;
        button.classList.add('oauth-enroll-button');
        button.classList.add.apply(button.classList, classes);
        button.textContent = label;
        button.addEventListener('click', handler);
        buttons.push(button);
      }

      return buttons;
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {Object} data Screen init payload, contains the signin frame
     * URL.
     */
    onBeforeShow: function(data) {
      if (Oobe.getInstance().forceKeyboardFlow) {
        // We run the tab remapping logic inside of the webview so that the
        // simulated tab events will use the webview tab-stops. Simulated tab
        // events created from the webui treat the entire webview as one tab
        // stop. Real tab events do not do this. See crbug.com/543865.
        $('oauth-enroll-auth-view').addContentScripts([{
          name: 'injectedTabHandler',
          matches: ['http://*/*', 'https://*/*'],
          js: { code: INJECTED_WEBVIEW_SCRIPT },
          run_at: 'document_start'
        }]);
      }

      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ENROLLMENT;
      $('progress-dots').hidden = true;
      var gaiaParams = {};
      gaiaParams.gaiaUrl = data.gaiaUrl;
      gaiaParams.clientId = data.clientId;
      gaiaParams.gaiaPath = 'embedded/setup/chromeos';
      gaiaParams.isNewGaiaFlow = true;
      gaiaParams.needPassword = false;
      if (data.management_domain) {
        gaiaParams.enterpriseDomain = data.management_domain;
        gaiaParams.emailDomain = data.management_domain;
      }
      gaiaParams.flow = data.flow;
      this.authenticator_.load(cr.login.Authenticator.AuthMode.DEFAULT,
                               gaiaParams);

      var modes = ['manual', 'forced', 'recovery'];
      for (var i = 0; i < modes.length; ++i) {
        this.classList.toggle('mode-' + modes[i],
                              data.enrollment_mode == modes[i]);
      }
      this.isCancelDisabled = true;
      this.showStep(STEP_SIGNIN);
    },

    onBeforeHide: function() {
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Shows attribute-prompt step with pre-filled asset ID and
     * location.
     */
    showAttributePromptStep: function(annotated_asset_id, annotated_location) {
      $('oauth-enroll-asset-id').value = annotated_asset_id;
      $('oauth-enroll-location').value = annotated_location;
      $('oauth-enroll-back-button').hidden = true;

      this.showStep(STEP_ATTRIBUTE_PROMPT);
    },

    /**
     * Cancels enrollment and drops the user back to the login screen.
     */
    cancel: function() {
      if (this.isCancelDisabled)
        return;
      this.isCancelDisabled = true;
      chrome.send('oauthEnrollClose', ['cancel']);
    },

    /**
     * Switches between the different steps in the enrollment flow.
     * @param {string} step the steps to show, one of "signin", "working",
     * "attribute-prompt", "error", "success".
     */
    showStep: function(step) {
      this.classList.toggle('oauth-enroll-state-' + this.currentStep_, false);
      this.classList.toggle('oauth-enroll-state-' + step, true);

      if (step == STEP_SIGNIN) {
        $('oauth-enroll-auth-view').focus();
      } else if (step == STEP_ERROR) {
        $('oauth-enroll-error-card').submitButton.focus();
      } else if (step == STEP_SUCCESS) {
        $('oauth-enroll-success-card').submitButton.focus();
      } else if (step == STEP_ATTRIBUTE_PROMPT) {
        $('oauth-enroll-asset-id').focus();
      } else if (step == STEP_ATTRIBUTE_PROMPT_ERROR) {
        $('oauth-enroll-attribute-prompt-error-card').submitButton.focus();
      }
      this.currentStep_ = step;
    },

    /**
     * Sets an error message and switches to the error screen.
     * @param {string} message the error message.
     * @param {boolean} retry whether the retry link should be shown.
     */
    showError: function(message, retry) {
      if (this.currentStep_ == STEP_ATTRIBUTE_PROMPT) {
        $('oauth-enroll-attribute-prompt-error-card').textContent = message;
        this.showStep(STEP_ATTRIBUTE_PROMPT_ERROR);
        return;
      }
      $('oauth-enroll-error-card').textContent = message;
      $('oauth-enroll-error-card').buttonLabel =
          retry ? loadTimeData.getString('oauthEnrollRetry') : '';
      this.showStep(STEP_ERROR);
    },

    doReload: function() {
      this.authenticator_.reload();
    },

    /**
     * Retries the enrollment process after an error occurred in a previous
     * attempt. This goes to the C++ side through |chrome| first to clean up the
     * profile, so that the next attempt is performed with a clean state.
     */
    doRetry_: function() {
      chrome.send('oauthEnrollRetry');
    },

    /**
     * Skips the device attribute update,
     * shows the successful enrollment step.
     */
    onSkipButtonClicked: function() {
      this.showStep(STEP_SUCCESS);
    },

    /**
     * Uploads the device attributes to server. This goes to C++ side through
     * |chrome| and launches the device attribute update negotiation.
     */
    onAttributesSubmitted: function() {
      chrome.send('oauthEnrollAttributes',
                  [$('oauth-enroll-asset-id').value,
                   $('oauth-enroll-location').value]);
    }
  };
});
