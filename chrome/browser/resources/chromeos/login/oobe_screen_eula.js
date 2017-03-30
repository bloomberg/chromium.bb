// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe eula screen implementation.
 */

login.createScreen('EulaScreen', 'eula', function() {
  var CONTEXT_KEY_USAGE_STATS_ENABLED = 'usageStatsEnabled';

  return {
    /** @override */
    decorate: function() {
      $('eula-chrome-credits-link').hidden = true;
      $('eula-chromeos-credits-link').hidden = true;
      $('stats-help-link').addEventListener('click', function(event) {
        chrome.send('eulaOnLearnMore');
      });
      $('installation-settings-link').addEventListener(
          'click', function(event) {
            chrome.send('eulaOnInstallationSettingsPopupOpened');
            $('popup-overlay').hidden = false;
            $('installation-settings-ok-button').focus();
          });
      $('installation-settings-ok-button').addEventListener(
          'click', function(event) {
            $('popup-overlay').hidden = true;
          });

      var self = this;
      $('usage-stats').addEventListener('click', function(event) {
        self.onUsageStatsClicked_($('usage-stats').checked);
        event.stopPropagation();
      });
      $('oobe-eula-md').screen = this;
    },

    /**
     * Event handler for $('usage-stats') click event.
     * @param {boolean} value $('usage-stats').checked value.
     */
    onUsageStatsClicked_: function(value) {
      this.context.set(CONTEXT_KEY_USAGE_STATS_ENABLED, value);
      this.commitContextChanges();
    },

    /**
     * Event handler that is invoked when 'chrome://terms' is loaded.
     */
    onFrameLoad: function() {
      $('accept-button').disabled = false;
      $('eula').classList.remove('eula-loading');
      // Initially, the back button is focused and the accept button is
      // disabled.
      // Move the focus to the accept button now but only if the user has not
      // moved the focus anywhere in the meantime.
      if (!$('back-button').blurred)
        $('accept-button').focus();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function() {
      this.setMDMode_();
      $('eula').classList.add('eula-loading');
      $('cros-eula-frame').onload = this.onFrameLoad;
      $('accept-button').disabled = true;
      $('cros-eula-frame').src = 'chrome://terms';
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('eulaScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {Array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var backButton = this.declareButton('back-button');
      backButton.textContent = loadTimeData.getString('back');
      buttons.push(backButton);

      var acceptButton = this.declareButton('accept-button');
      acceptButton.disabled = true;
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent = loadTimeData.getString('acceptAgreement');
      acceptButton.addEventListener('click', function(e) {
        $('eula').classList.add('loading');  // Mark EULA screen busy.
        Oobe.clearErrors();
        e.stopPropagation();
      });
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      if (loadTimeData.getString('newOobeUI') == 'on')
        return $('oobe-eula-md');

      return $('accept-button').disabled ? $('back-button') :
                                           $('accept-button');
    },

    enableKeyboardFlow: function() {
      $('eula-chrome-credits-link').hidden = false;
      $('eula-chromeos-credits-link').hidden = false;
      $('eula-chrome-credits-link').addEventListener('click',
          function(event) {
            chrome.send('eulaOnChromeCredits');
          });
      $('eula-chromeos-credits-link').addEventListener('click',
          function(event) {
            chrome.send('eulaOnChromeOSCredits');
          });
    },

    /**
     * This method takes care of switching to material-design OOBE.
     * @private
     */
    setMDMode_: function() {
      var useMDOobe = (loadTimeData.getString('newOobeUI') == 'on');
      $('oobe-eula-md').hidden = !useMDOobe;
      $('oobe-eula').hidden = useMDOobe;
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      this.setMDMode_();

      $('oobe-eula-md').updateLocalizedContent();

      // Force iframes to refresh. It's only available method because we have
      // no access to iframe.contentWindow.
      if ($('cros-eula-frame').src) {
        $('cros-eula-frame').src = $('cros-eula-frame').src;
      }
      if ($('oem-eula-frame').src) {
        $('oem-eula-frame').src = $('oem-eula-frame').src;
      }
    },
  };
});

