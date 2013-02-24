// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Terms of Service screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Create a new Terms of Service screen.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TermsOfServiceScreen = cr.ui.define('div');

  /**
   * Register with Oobe.
   */
  TermsOfServiceScreen.register = function() {
    var screen = $('terms-of-service');
    TermsOfServiceScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  /**
   * Updates headings on the screen to indicate that the Terms of Service being
   * shown belong to |domain|.
   * @param {string} domain The domain whose Terms of Service are being shown.
   */
  TermsOfServiceScreen.setDomain = function(domain) {
    $('tos-heading').textContent =
        loadTimeData.getStringF('termsOfServiceScreenHeading', domain);
    $('tos-subheading').textContent =
        loadTimeData.getStringF('termsOfServiceScreenSubheading', domain);
    $('tos-content-heading').textContent =
        loadTimeData.getStringF('termsOfServiceContentHeading', domain);
  };

  /**
   * Displays the given |termsOfService|, enables the accept button and moves
   * the focus to it.
   * @param {string} termsOfService The terms of service, as plain text.
   */
  TermsOfServiceScreen.setTermsOfService = function(termsOfService) {
    $('terms-of-service').classList.remove('tos-loading');
    $('tos-content-main').textContent = termsOfService;
    $('tos-accept-button').disabled = false;
    // Initially, the back button is focused and the accept button is disabled.
    // Move the focus to the accept button now but only if the user has not
    // moved the focus anywhere in the meantime.
    if (!$('tos-back-button').blurred)
      $('tos-accept-button').focus();
  };

  TermsOfServiceScreen.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
    },

    /**
     * Buttons in Oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var backButton = this.ownerDocument.createElement('button');
      backButton.id = 'tos-back-button';
      backButton.textContent =
          loadTimeData.getString('termsOfServiceBackButton');
      backButton.addEventListener('click', function(event) {
        $('tos-back-button').disabled = true;
        $('tos-accept-button').disabled = true;
        chrome.send('termsOfServiceBack');
      });
      backButton.addEventListener('blur', function(event) {
        this.blurred = true;
      });
      buttons.push(backButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'tos-accept-button';
      acceptButton.disabled = true;
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent =
          loadTimeData.getString('termsOfServiceAcceptButton');
      acceptButton.addEventListener('click', function(event) {
        $('tos-back-button').disabled = true;
        $('tos-accept-button').disabled = true;
        chrome.send('termsOfServiceAccept');
      });
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('tos-accept-button').disabled ? $('tos-back-button') :
                                               $('tos-accept-button');
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
    },
  };

  return {
    TermsOfServiceScreen: TermsOfServiceScreen
  };
});
