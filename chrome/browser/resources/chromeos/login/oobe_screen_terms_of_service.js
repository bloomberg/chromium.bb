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
        localStrings.getStringF('termsOfServiceScreenHeading', domain);
    $('tos-subheading').textContent =
        localStrings.getStringF('termsOfServiceScreenSubheading', domain);
    $('tos-content-heading').textContent =
        localStrings.getStringF('termsOfServiceContentHeading', domain);
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
          localStrings.getString('termsOfServiceBackButton');
      backButton.addEventListener('click', function(event) {
        $('tos-back-button').disabled = true;
        $('tos-accept-button').disabled = true;
        chrome.send('termsOfServiceBack');
      });
      buttons.push(backButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'tos-accept-button';
      acceptButton.disabled = true;
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent =
          localStrings.getString('termsOfServiceAcceptButton');
      acceptButton.addEventListener('click', function(event) {
        $('tos-back-button').disabled = true;
        $('tos-accept-button').disabled = true;
        chrome.send('termsOfServiceAccept');
      });
      buttons.push(acceptButton);

      return buttons;
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
