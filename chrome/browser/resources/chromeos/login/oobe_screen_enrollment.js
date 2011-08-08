// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var EnrollmentScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  EnrollmentScreen.register = function() {
    var screen = $('enrollment');
    EnrollmentScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  EnrollmentScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      // TODO(altimofeev): add accelerators for the Enterprise Enrollment
      // screen.
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('loginHeader');
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      // Reload the gaia frame.
      $('gaia-local-login').src = 'chrome://oobe/gaialogin';
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var closeButton = this.ownerDocument.createElement('button');
      closeButton.id = 'close-button';
      closeButton.textContent = localStrings.getString('confirmationClose');
      closeButton.addEventListener('click', function(e) {
        chrome.send('confirmationClose', []);
      });
      buttons.push(closeButton);

      return buttons;
    }
  };

  /**
   * Shows confirmation screen for the enterprise enrollment.
   */
  EnrollmentScreen.showConfirmationScreen = function() {
    $('enroll-login-screen').hidden = true;
    $('enroll-confirmation-screen').hidden = false;
    $('close-button').classList.add('visible');
  };

  return {
    EnrollmentScreen: EnrollmentScreen
  };
});
