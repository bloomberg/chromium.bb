// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe eula screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var EulaScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  EulaScreen.register = function() {
    var screen = $('eula');
    EulaScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  EulaScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      $('stats-help-link').addEventListener('click', function(event) {
        chrome.send('eulaOnLearnMore');
      });
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('eulaScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var backButton = this.ownerDocument.createElement('button');
      backButton.id = 'back-button';
      backButton.textContent = localStrings.getString('back');
      backButton.addEventListener('click', function(e) {
        chrome.send('eulaOnExit', [false, $('usage-stats').checked]);
        e.stopPropagation();
      });
      buttons.push(backButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'accept-button';
      acceptButton.textContent = localStrings.getString('acceptAgreement');
      acceptButton.addEventListener('click', function(e) {
        chrome.send('eulaOnExit', [true, $('usage-stats').checked]);
        e.stopPropagation();
      });
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('accept-button');
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      if ($('cros-eula-frame').src != '')
        $('cros-eula-frame').src = $('cros-eula-frame').src;
      if ($('oem-eula-frame').src != '')
        $('oem-eula-frame').src = $('oem-eula-frame').src;
    }
  };

  return {
    EulaScreen: EulaScreen
  };
});
