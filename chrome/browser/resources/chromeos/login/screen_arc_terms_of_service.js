// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Arc Terms of Service screen implementation.
 */

login.createScreen('ArcTermsOfServiceScreen', 'arc-tos',
  function() { return {
    EXTERNAL_API: [
      'setMetricsMode',
      'setBackupAndRestoreMode',
      'setLocationServicesMode',
      'setCountryCode'
    ],

    /** @override */
    decorate: function(element) {
      var closeButtons = document.querySelectorAll('.arc-overlay-close-button');
      for (var i = 0; i < closeButtons.length; i++) {
        closeButtons[i].addEventListener('click', this.hideOverlay.bind(this));
      }

      var termsView = $('arc-tos-view');
      var requestFilter = {
        urls: ['<all_urls>'],
        types: ['main_frame']
      };

      termsView.request.onErrorOccurred.addListener(
          this.onTermsViewErrorOccurred.bind(this), requestFilter);
      termsView.addEventListener('contentload',
          this.onTermsViewContentLoad.bind(this));

      // Open links from webview in overlay dialog.
      var self = this;
      termsView.addEventListener('newwindow', function(event) {
        event.preventDefault();
        self.showUrlOverlay(event.targetUrl);
      });

      termsView.addContentScripts([
        { name: 'postProcess',
          matches: ['https://play.google.com/*'],
          css: { files: ['playstore.css'] },
          js: { files: ['playstore.js'] },
          run_at: 'document_end'
        }]);

      $('arc-policy-link').onclick = function() {
        termsView.executeScript(
            {code: 'getPrivacyPolicyLink();'},
            function(results) {
              if (results && results.length == 1 &&
                  typeof results[0] == 'string') {
                self.showUrlOverlay(results[0]);
              } else {
                self.showUrlOverlay('https://www.google.com/policies/privacy/');
              }
            });
      };

      this.updateLocalizedContent();
    },

    /**
     * Sets current metrics mode.
     * @param {string} text Describes current metrics state.
     * @param {boolean} visible If metrics text is visible.
     */
    setMetricsMode: function(text, visible) {
      $('arc-text-metrics').hidden = !visible;
      $('arc-text-metrics').innerHTML = text;

      if (!visible) {
        return;
      }

      var self = this;
      var leanMoreStatisticsText =
          loadTimeData.getString('arcLearnMoreStatistics');
      $('learn-more-link-metrics').onclick = function() {
        self.showLearnMoreOverlay(leanMoreStatisticsText);
      };
    },

    /**
     * Applies current enabled/managed state to checkbox and text.
     * @param {string} checkBoxId Id of checkbox to set on/off.
     * @param {string} textId Id of text to set enabled state.
     * @param {boolean} enabled Defines the value of the checkbox.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setPreference(checkBoxId, textId, enabled, managed) {
      $(checkBoxId).checked = enabled;
      $(checkBoxId).disabled = managed;
      $(textId).disabled = managed;
    },

    /**
     * Sets current backup and restore mode.
     * @param {boolean} enabled Defines the value for backup and restore
     *                          checkbox.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setBackupAndRestoreMode: function(enabled, managed) {
      this.setPreference('arc-enable-backup-restore',
                         'arc-text-backup-restore',
                         enabled, managed);
    },

    /**
     * Sets current usage of location service opt in mode.
     * @param {boolean} enabled Defines the value for location service opt in.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setLocationServicesMode: function(enabled, managed) {
      this.setPreference('arc-enable-location-service',
                         'arc-text-location-service',
                         enabled, managed);
    },

    /**
     * Sets current country code for ToS.
     * @param {string} countryCode Country code based on current timezone.
     */
    setCountryCode: function(countryCode) {
      var scriptSetCountryCode = 'document.countryCode = \'' +
          countryCode.toLowerCase() + '\';';
      var termsView = $('arc-tos-view');
      termsView.removeContentScripts(['preProcess']);
      termsView.addContentScripts([
          { name: 'preProcess',
            matches: ['https://play.google.com/*'],
            js: { code: scriptSetCountryCode },
            run_at: 'document_start'
          }]);

      if (!$('arc-tos').hidden) {
        this.reloadPlayStore();
      }
    },

    /**
     * Buttons in Oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var skipButton = this.ownerDocument.createElement('button');
      skipButton.id = 'arc-tos-skip-button';
      skipButton.disabled = this.classList.contains('arc-tos-loading');
      skipButton.classList.add('preserve-disabled-state');
      skipButton.textContent =
          loadTimeData.getString('arcTermsOfServiceSkipButton');
      skipButton.addEventListener('click', function(event) {
        $('arc-tos-skip-button').disabled = true;
        $('arc-tos-accept-button').disabled = true;
        chrome.send('arcTermsOfServiceSkip');
      });
      buttons.push(skipButton);

      var retryButton = this.ownerDocument.createElement('button');
      retryButton.id = 'arc-tos-retry-button';
      retryButton.textContent =
          loadTimeData.getString('arcTermsOfServiceRetryButton');
      retryButton.addEventListener('click', this.reloadPlayStore.bind(this));
      buttons.push(retryButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'arc-tos-accept-button';
      acceptButton.disabled = this.classList.contains('arc-tos-loading');
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent =
          loadTimeData.getString('arcTermsOfServiceAcceptButton');
      acceptButton.addEventListener('click', function(event) {
        $('arc-tos-skip-button').disabled = true;
        $('arc-tos-accept-button').disabled = true;

        var isBackupRestoreEnabled = $('arc-enable-backup-restore').checked;
        var isLocationServiceEnabled = $('arc-enable-location-service').checked;

        chrome.send('arcTermsOfServiceAccept',
            [isBackupRestoreEnabled, isLocationServiceEnabled]);
      });
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('arc-tos-accept-button').disabled ? $('arc-tos-skip-button') :
                                                   $('arc-tos-accept-button');
    },

    /**
     * Sets learn more content text and shows it as overlay dialog.
     * @param {string} content HTML formatted text to show.
     */
    showLearnMoreOverlay: function(content) {
      $('arc-learn-more-content').innerHTML = content;
      $('arc-tos-overlay-content-text').hidden = false;
      $('arc-tos-overlay-text').hidden = false;
    },

    /**
     * Opens external URL in popup overlay.
     * @param {string} targetUrl URL to open.
     */
    showUrlOverlay: function(targetUrl) {
      $('arc-tos-overlay-webview').src = targetUrl;
      $('arc-tos-overlay-url').hidden = false;
    },

    /**
     * Hides overlay dialog.
     */
    hideOverlay: function() {
      $('arc-tos-overlay-text').hidden = true;
      $('arc-tos-overlay-url').hidden = true;
    },

    /**
     * Reloads Play Store.
     */
    reloadPlayStore: function() {
      this.termsError = false;
      var termsView = $('arc-tos-view');
      termsView.src = 'https://play.google.com/about/play-terms.html';
      this.classList.remove('arc-tos-loaded');
      this.classList.remove('error');
      this.classList.add('arc-tos-loading');

      $('arc-tos-accept-button').disabled = true;
      $('arc-tos-skip-button').disabled = true;
    },

    /**
     * Handles event when terms view is loaded.
     */
    onTermsViewContentLoad: function() {
      if (this.termsError) {
        return;
      }

      this.classList.remove('arc-tos-loading');
      this.classList.remove('error');
      this.classList.add('arc-tos-loaded');

      var acceptButton = $('arc-tos-accept-button');
      var skipButton = $('arc-tos-skip-button');

      skipButton.disabled = false;
      acceptButton.disabled = false;
      acceptButton.focus();

      var termsView = $('arc-tos-view');
      var termsViewContainer = $('arc-tos-view-container');
      var setTermsHeight = function() {
        // Reset terms-view height in order to stabilize style computation.
        // For some reason, child webview affects final result.
        termsView.style.height = '0px';
        var style = window.getComputedStyle(termsViewContainer, null);
        var height = style.getPropertyValue('height');
        termsView.style.height = height;
      };
      setTimeout(setTermsHeight, 0);
    },

    /**
     * Handles event when terms view cannot be loaded.
     */
    onTermsViewErrorOccurred: function(details) {
      this.termsError = true;
      this.classList.remove('arc-tos-loading');
      this.classList.remove('arc-tos-loaded');
      this.classList.add('error');

      $('arc-tos-retry-button').focus();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;

      // Reload caption image in case it was not loaded during the
      // initialization phase.
      $('arc-tos-logo').src =
        'https://play.google.com/about/images/play_logo.png';

      this.hideOverlay();
      this.reloadPlayStore();
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      var self = this;

      var leanMoreBackupAndRestoreText =
          loadTimeData.getString('arcLearnMoreBackupAndRestore');
      $('learn-more-link-backup-restore').onclick = function() {
        self.showLearnMoreOverlay(leanMoreBackupAndRestoreText);
      };

      var leanMoreLocationServiceText =
          loadTimeData.getString('arcLearnMoreLocationService');
      $('learn-more-link-location-service').onclick = function() {
        self.showLearnMoreOverlay(leanMoreLocationServiceText);
      };
    }
  };
});
