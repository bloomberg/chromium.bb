// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * Switches between the different steps in the enrollment flow.
   * @param screen {string} the steps to show, one of "signin", "working",
   * "error", "success".
   */
  OAuthEnrollmentScreen.showStep = function(step) {
    $('oauth-enrollment').showStep(step);
  };

  /**
   * Sets an error message and switches to the error screen.
   * @param message {string} the error message.
   * @param retry {bool} whether the retry link should be shown.
   */
  OAuthEnrollmentScreen.showError = function(message, retry) {
    $('oauth-enrollment').showError(message, retry);
  };

  OAuthEnrollmentScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * URL to load in the sign in frame.
     */
    signin_url_ : null,

    /**
     * Enrollment steps with names and buttons to show.
     */
    steps_ : [
      { name: 'signin',
        button: 'cancel' },
      { name: 'working',
        button: 'cancel' },
      { name: 'error',
        button: 'cancel' },
      { name: 'success',
        button: 'done',
        focusButton: true },
    ],

    /** @inheritDoc */
    decorate: function() {
      $('oauth-enroll-error-retry').addEventListener('click', function() {
        chrome.send('oauthEnrollRetry', []);
      });
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('oauthEnrollScreenTitle');
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
          localStrings.getString('oauthEnrollCancel');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('oauthEnrollClose', []);
      });
      buttons.push(cancelButton);

      var doneButton = this.ownerDocument.createElement('button');
      doneButton.id = 'oauth-enroll-done-button';
      doneButton.hidden = true;
      doneButton.textContent =
          localStrings.getString('oauthEnrollDone');
      doneButton.addEventListener('click', function(e) {
        chrome.send('oauthEnrollClose', []);
      });
      buttons.push(doneButton);

      return buttons;
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param data {dictionary} Screen init payload, contains the signin frame
     * URL.
     */
    onBeforeShow: function(data) {
      var url = data.signin_url;
      if (data.gaiaOrigin)
        url += '?gaiaOrigin=' + encodeURIComponent(data.gaiaOrigin);
      this.signin_url_ = url;
      $('oauth-enroll-signin-frame').contentWindow.location.href =
          this.signin_url_;
      this.showStep('signin');
    },

    /**
     * Cancels enrollment and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('oauthEnrollClose', []);
    },

    /**
     * Switches between the different steps in the enrollment flow.
     * @param step {string} the steps to show, one of "signin", "working",
     * "error", "success".
     */
    showStep: function(step) {
      $('oauth-enroll-cancel-button').hidden = true;
      $('oauth-enroll-done-button').hidden = true;
      for (var i = 0; i < this.steps_.length; i++) {
        var theStep = this.steps_[i];
        var active = (theStep.name == step);
        $('oauth-enroll-step-' + theStep.name).hidden = !active;
        if (active) {
          var button = $('oauth-enroll-' + theStep.button + '-button');
          button.hidden = false;
          if (theStep.focusButton)
            button.focus();
        }
      }
    },

    /**
     * Sets an error message and switches to the error screen.
     * @param message {string} the error message.
     * @param retry {bool} whether the retry link should be shown.
     */
    showError: function(message, retry) {
      $('oauth-enroll-error-message').textContent = message;
      $('oauth-enroll-error-retry').hidden = !retry;
      this.showStep('error');
    },

    /**
     * Checks if a given HTML5 message comes from the URL loaded into the signin
     * frame.
     * @param m {object} HTML5 message.
     * @type {bool} whether the message comes from the signin frame.
     */
    isSigninMessage_: function(m) {
      return this.signin_url_ != null &&
          this.signin_url_.indexOf(m.origin) == 0 &&
          m.source == $('oauth-enroll-signin-frame').contentWindow;
    },

    /**
     * Event handler for HTML5 messages.
     * @param m {object} HTML5 message.
     */
    onMessage_: function(m) {
      var msg = m.data;
      if (msg.method == 'completeLogin' && this.isSigninMessage_(m))
        chrome.send('oauthEnrollCompleteLogin', [ msg.email, msg.password ]);
    }
  };

  return {
    OAuthEnrollmentScreen: OAuthEnrollmentScreen
  };
});
