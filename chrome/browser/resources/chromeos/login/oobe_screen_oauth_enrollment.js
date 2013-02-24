// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const */ STEP_SIGNIN = 'signin';
/** @const */ STEP_WORKING = 'working';
/** @const */ STEP_ERROR = 'error';
/** @const */ STEP_EXPLAIN = 'explain';
/** @const */ STEP_SUCCESS = 'success';

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var OAuthEnrollmentScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  OAuthEnrollmentScreen.register = function() {
    var screen = $('oauth-enrollment');
    OAuthEnrollmentScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
    window.addEventListener('message',
                            screen.onMessage_.bind(screen), false);
  };

  /**
   * Sets the |isAutoEnrollment| flag of the OAuthEnrollmentScreen class and
   * updates the UI.
   * @param {boolean} is_auto_enrollment the new value of the flag.
   */
  OAuthEnrollmentScreen.setIsAutoEnrollment = function(is_auto_enrollment) {
    $('oauth-enrollment').setIsAutoEnrollment(is_auto_enrollment);
  };

  /**
   * Switches between the different steps in the enrollment flow.
   * @param {string} screen the steps to show, one of "signin", "working",
   * "error", "success".
   */
  OAuthEnrollmentScreen.showStep = function(step) {
    $('oauth-enrollment').showStep(step);
  };

  /**
   * Sets an error message and switches to the error screen.
   * @param {string} message the error message.
   * @param {boolean} retry whether the retry link should be shown.
   */
  OAuthEnrollmentScreen.showError = function(message, retry) {
    $('oauth-enrollment').showError(message, retry);
  };

  /**
   * Sets a progressing message and switches to the working screen.
   * @param {string} message the progress message.
   */

  OAuthEnrollmentScreen.showWorking = function(message) {
    $('oauth-enrollment').showWorking(message);
  };

  OAuthEnrollmentScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * URL to load in the sign in frame.
     */
    signInUrl_: null,

    /**
     * Whether this is a manual or auto enrollment.
     */
    isAutoEnrollment_: false,

    /**
     * Enrollment steps with names and buttons to show.
     */
    steps_: null,

    /**
     * Dialog to confirm that auto-enrollment should really be cancelled.
     * This is only created the first time it's used.
     */
    confirmDialog_: null,

    /**
     * The current step. This is the last value passed to showStep().
     */
    currentStep_: null,

    /** @override */
    decorate: function() {
      $('oauth-enroll-error-retry').addEventListener('click',
                                                     this.doRetry_.bind(this));
      var links = document.querySelectorAll('.oauth-enroll-explain-link');
      for (var i = 0; i < links.length; i++) {
        links[i].addEventListener('click',
                                  this.showStep.bind(this, STEP_EXPLAIN));
      }
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

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'oauth-enroll-cancel-button';
      cancelButton.textContent =
          loadTimeData.getString('oauthEnrollCancel');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('oauthEnrollClose', ['cancel']);
      });
      buttons.push(cancelButton);

      var tryAgainButton = this.ownerDocument.createElement('button');
      tryAgainButton.id = 'oauth-enroll-try-again-button';
      tryAgainButton.hidden = true;
      tryAgainButton.textContent =
          loadTimeData.getString('oauthEnrollRetry');
      tryAgainButton.addEventListener('click', this.doRetry_.bind(this));
      buttons.push(tryAgainButton);

      var explainButton = this.ownerDocument.createElement('button');
      explainButton.id = 'oauth-enroll-explain-button';
      explainButton.hidden = true;
      explainButton.textContent =
          loadTimeData.getString('oauthEnrollExplainButton');
      explainButton.addEventListener('click', this.doRetry_.bind(this));
      buttons.push(explainButton);

      var doneButton = this.ownerDocument.createElement('button');
      doneButton.id = 'oauth-enroll-done-button';
      doneButton.hidden = true;
      doneButton.textContent =
          loadTimeData.getString('oauthEnrollDone');
      doneButton.addEventListener('click', function(e) {
        chrome.send('oauthEnrollClose', ['done']);
      });
      buttons.push(doneButton);

      return buttons;
    },

    /**
     * Changes the auto-enrollment flag and updates the UI.
     */
    setIsAutoEnrollment: function(is_auto_enrollment) {
      this.isAutoEnrollment_ = is_auto_enrollment;
      // The cancel button is not available during auto-enrollment.
      var cancel = this.isAutoEnrollment_ ? null : 'cancel';
      // During auto-enrollment the user must try again from the error screen.
      var error_cancel = this.isAutoEnrollment_ ? 'try-again' : 'cancel';
      this.steps_ = [
        { name: STEP_SIGNIN,
          button: cancel },
        { name: STEP_WORKING,
          button: cancel },
        { name: STEP_ERROR,
          button: error_cancel,
          focusButton: this.isAutoEnrollment_ },
        { name: STEP_EXPLAIN,
          button: 'explain',
          focusButton: true },
        { name: STEP_SUCCESS,
          button: 'done',
          focusButton: true },
      ];

      var links = document.querySelectorAll('.oauth-enroll-explain-link');
      for (var i = 0; i < links.length; i++)
        links[i].hidden = !this.isAutoEnrollment_;
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {Object} data Screen init payload, contains the signin frame
     * URL.
     */
    onBeforeShow: function(data) {
      var url = data.signin_url;
      url += '?gaiaOrigin=' + encodeURIComponent(data.gaiaOrigin);
      if (data.gaiaUrlBase)
        url += '&gaiaUrlPath=' + encodeURIComponent(data.gaiaUrlPath);
      if (data.test_email) {
        url += '&test_email=' + encodeURIComponent(data.test_email);
        url += '&test_password=' + encodeURIComponent(data.test_password);
      }
      this.signInUrl_ = url;
      this.setIsAutoEnrollment(data.is_auto_enrollment);

      $('oauth-enroll-signin-frame').contentWindow.location.href =
          this.signInUrl_;

      this.showStep(STEP_SIGNIN);
    },

    /**
     * Cancels enrollment and drops the user back to the login screen.
     */
    cancel: function() {
      if (!this.isAutoEnrollment_)
        chrome.send('oauthEnrollClose', ['cancel']);
    },

    /**
     * Switches between the different steps in the enrollment flow.
     * @param {string} step the steps to show, one of "signin", "working",
     * "error", "success".
     */
    showStep: function(step) {
      this.currentStep_ = step;
      $('oauth-enroll-cancel-button').hidden = true;
      $('oauth-enroll-try-again-button').hidden = true;
      $('oauth-enroll-explain-button').hidden = true;
      $('oauth-enroll-done-button').hidden = true;
      for (var i = 0; i < this.steps_.length; i++) {
        var theStep = this.steps_[i];
        var active = (theStep.name == step);
        $('oauth-enroll-step-' + theStep.name).hidden = !active;
        if (active && theStep.button) {
          var button = $('oauth-enroll-' + theStep.button + '-button');
          button.hidden = false;
          if (theStep.focusButton)
            button.focus();
        }
      }
    },

    /**
     * Sets an error message and switches to the error screen.
     * @param {string} message the error message.
     * @param {boolean} retry whether the retry link should be shown.
     */
    showError: function(message, retry) {
      $('oauth-enroll-error-message').textContent = message;
      $('oauth-enroll-error-retry').hidden = !retry || this.isAutoEnrollment_;
      this.showStep(STEP_ERROR);
    },

    /**
     * Sets a progressing message and switches to the working screen.
     * @param {string} message the progress message.
     */
    showWorking: function(message) {
      $('oauth-enroll-working-message').textContent = message;
      this.showStep(STEP_WORKING);
    },

    /**
     * Handler for cancellations of an enforced auto-enrollment.
     */
    cancelAutoEnrollment: function() {
      // The dialog to confirm cancellation of auto-enrollment is only shown
      // if this is an auto-enrollment, and if the user is currently in the
      // 'explain' step.
      if (!this.isAutoEnrollment_ || this.currentStep_ !== STEP_EXPLAIN)
        return;
      if (!this.confirmDialog_) {
        this.confirmDialog_ = new cr.ui.dialogs.ConfirmDialog(document.body);
        this.confirmDialog_.setOkLabel(
            loadTimeData.getString('oauthEnrollCancelAutoEnrollmentConfirm'));
        this.confirmDialog_.setCancelLabel(
            loadTimeData.getString('oauthEnrollCancelAutoEnrollmentGoBack'));
        this.confirmDialog_.setInitialFocusOnCancel();
      }
      this.confirmDialog_.show(
          loadTimeData.getString('oauthEnrollCancelAutoEnrollmentReally'),
          this.onConfirmCancelAutoEnrollment_.bind(this));
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
     * Handler for confirmation of cancellation of auto-enrollment.
     */
    onConfirmCancelAutoEnrollment_: function() {
      chrome.send('oauthEnrollClose', ['autocancel']);
    },

    /**
     * Checks if a given HTML5 message comes from the URL loaded into the signin
     * frame.
     * @param {Object} m HTML5 message.
     * @type {boolean} whether the message comes from the signin frame.
     */
    isSigninMessage_: function(m) {
      return this.signInUrl_ != null &&
          this.signInUrl_.indexOf(m.origin) == 0 &&
          m.source == $('oauth-enroll-signin-frame').contentWindow;
    },

    /**
     * Event handler for HTML5 messages.
     * @param {Object} m HTML5 message.
     */
    onMessage_: function(m) {
      var msg = m.data;
      if (msg.method == 'completeLogin' && this.isSigninMessage_(m))
        chrome.send('oauthEnrollCompleteLogin', [msg.email, msg.password]);
    }
  };

  return {
    OAuthEnrollmentScreen: OAuthEnrollmentScreen
  };
});
