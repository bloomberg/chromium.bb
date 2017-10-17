// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe ARC Terms of Service screen implementation.
 */

login.createScreen('ArcTermsOfServiceScreen', 'arc-tos', function() {
  return {
    EXTERNAL_API: [
      'setMetricsMode', 'setBackupAndRestoreMode', 'setLocationServicesMode',
      'loadPlayStoreToS'
    ],

    /** @override */
    decorate: function(element) {
      // Valid newOobeUI is not available at this time.
      this.countryCode_ = null;
      this.language_ = null;
    },

    /**
     * Returns current language that can be updated in OOBE flow. If OOBE flow
     * does not exist then use navigator.language.
     *
     * @private
     */
    getCurrentLanguage_: function() {
      var languageList = loadTimeData.getValue('languageList');
      if (languageList) {
        var language = Oobe.getSelectedValue(languageList);
        if (language) {
          return language;
        }
      }
      return navigator.language;
    },

    /**
     * Returns true if page was intialized.
     *
     * @private
     */
    isPageReady_: function() {
      return typeof this.useMDOobe !== 'undefined';
    },

    /**
     * Makes sure that UI is initialized and MD mode is correctly set.
     *
     * @private
     */
    setMDMode_: function() {
      var useMDOobe = (loadTimeData.getString('newOobeUI') == 'on');
      if (this.isPageReady_() && this.useMDOobe == useMDOobe) {
        return;
      }

      this.useMDOobe = useMDOobe;
      $('arc-tos-md').screen = this;
      $('arc-tos-md').hidden = !this.useMDOobe;
      $('arc-tos-legacy').hidden = this.useMDOobe;
      if (this.useMDOobe) {
        $('arc-tos').setAttribute('md-mode', 'true');
      } else {
        $('arc-tos').removeAttribute('md-mode');
      }

      var closeButtons = document.querySelectorAll('.arc-overlay-close-button');
      for (var i = 0; i < closeButtons.length; i++) {
        closeButtons[i].addEventListener('click', this.hideOverlay.bind(this));
      }

      var termsView = this.getElement_('arc-tos-view');
      var requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};

      termsView.request.onErrorOccurred.addListener(
          this.onTermsViewErrorOccurred.bind(this), requestFilter);
      termsView.addEventListener(
          'contentload', this.onTermsViewContentLoad.bind(this));

      // Open links from webview in overlay dialog.
      var self = this;
      termsView.addEventListener('newwindow', function(event) {
        event.preventDefault();
        self.showUrlOverlay(event.targetUrl);
      });

      termsView.addContentScripts([{
        name: 'postProcess',
        matches: ['https://play.google.com/*'],
        css: {files: ['playstore.css']},
        js: {files: ['playstore.js']},
        run_at: 'document_end'
      }]);

      this.getElement_('arc-policy-link').onclick = function() {
        termsView.executeScript(
            {code: 'getPrivacyPolicyLink();'}, function(results) {
              if (results && results.length == 1 &&
                  typeof results[0] == 'string') {
                self.showUrlOverlay(results[0]);
              } else {
                var defaultLink = 'https://www.google.com/intl/' +
                    self.getCurrentLanguage_() + '/policies/privacy/';
                self.showUrlOverlay(defaultLink);
              }
            });
      };

      // Update the screen size after setup layout.
      Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Sets current metrics mode.
     * @param {string} text Describes current metrics state.
     * @param {boolean} visible If metrics text is visible.
     */
    setMetricsMode: function(text, visible) {
      var metrics = this.getElement_('arc-text-metrics');
      metrics.innerHTML = text;
      // This element is wrapped by label in legacy mode and by div in MD mode.
      metrics.parentElement.hidden = !visible;

      if (!visible) {
        return;
      }

      var self = this;
      var leanMoreStatisticsText =
          loadTimeData.getString('arcLearnMoreStatistics');
      metrics.querySelector('#learn-more-link-metrics').onclick = function() {
        self.showLearnMoreOverlay(leanMoreStatisticsText);
      };
    },

    /**
     * Applies current enabled/managed state to checkbox and text.
     * @param {string} checkBoxId Id of checkbox to set on/off.
     * @param {boolean} enabled Defines the value of the checkbox.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setPreference(checkBoxId, enabled, managed) {
      var preference = this.getElement_(checkBoxId);
      preference.checked = enabled;
      preference.disabled = managed;
      preference.parentElement.disabled = managed;
    },

    /**
     * Sets current backup and restore mode.
     * @param {boolean} enabled Defines the value for backup and restore
     *                          checkbox.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setBackupAndRestoreMode: function(enabled, managed) {
      this.setPreference('arc-enable-backup-restore', enabled, managed);
    },

    /**
     * Sets current usage of location service opt in mode.
     * @param {boolean} enabled Defines the value for location service opt in.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setLocationServicesMode: function(enabled, managed) {
      this.setPreference('arc-enable-location-service', enabled, managed);
    },

    /**
     * Loads Play Store ToS in case country code has been changed or previous
     * attempt failed.
     * @param {string} countryCode Country code based on current timezone.
     */
    loadPlayStoreToS: function(countryCode) {
      // Make sure page is initialized for login mode. For OOBE mode, page is
      // initialized as result of handling updateLocalizedContent.
      this.setMDMode_();

      var language = this.getCurrentLanguage_();
      countryCode = countryCode.toLowerCase();

      if (this.language_ && this.language_ == language && this.countryCode_ &&
          this.countryCode_ == countryCode &&
          !this.classList.contains('error')) {
        return;
      }

      // Store current ToS parameters.
      this.language_ = language;
      this.countryCode_ = countryCode;

      var scriptSetParameters =
          'document.countryCode = \'' + countryCode + '\';';
      scriptSetParameters += 'document.language = \'' + language + '\';';
      if (this.useMDOobe) {
        scriptSetParameters += 'document.viewMode = \'large-view\';';
      }

      var termsView = this.getElement_('arc-tos-view');
      termsView.removeContentScripts(['preProcess']);
      termsView.addContentScripts([{
        name: 'preProcess',
        matches: ['https://play.google.com/*'],
        js: {code: scriptSetParameters},
        run_at: 'document_start'
      }]);

      // Try to use currently loaded document first.
      var self = this;
      if (termsView.src != '' && this.classList.contains('arc-tos-loaded')) {
        var navigateScript = 'processLangZoneTerms(true, \'' + language +
            '\', \'' + countryCode + '\');';
        termsView.executeScript({code: navigateScript}, function(results) {
          if (!results || results.length != 1 ||
              typeof results[0] !== 'boolean' || !results[0]) {
            self.reloadPlayStoreToS();
          }
        });
      } else {
        this.reloadPlayStoreToS();
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
      skipButton.addEventListener('click', this.onSkip.bind(this));
      buttons.push(skipButton);

      var retryButton = this.ownerDocument.createElement('button');
      retryButton.id = 'arc-tos-retry-button';
      retryButton.textContent =
          loadTimeData.getString('arcTermsOfServiceRetryButton');
      retryButton.addEventListener('click', this.reloadPlayStoreToS.bind(this));
      buttons.push(retryButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'arc-tos-accept-button';
      acceptButton.disabled = this.classList.contains('arc-tos-loading');
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent =
          loadTimeData.getString('arcTermsOfServiceAcceptButton');
      acceptButton.addEventListener('click', this.onAccept.bind(this));
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Handles Accept button click.
     */
    onAccept: function() {
      this.enableButtons_(false);

      var isBackupRestoreEnabled =
          this.getElement_('arc-enable-backup-restore').checked;
      var isLocationServiceEnabled =
          this.getElement_('arc-enable-location-service').checked;

      chrome.send(
          'arcTermsOfServiceAccept',
          [isBackupRestoreEnabled, isLocationServiceEnabled]);
    },

    /**
     * Handles Retry button click.
     */
    onSkip: function() {
      this.enableButtons_(false);

      chrome.send('arcTermsOfServiceSkip');
    },

    /**
     * Enables/Disables set of buttons: Accept, Skip, Retry.
     * @param {boolean} enable Buttons are enabled if set to true.
     *
     * @private
     */
    enableButtons_: function(enable) {
      $('arc-tos-skip-button').disabled = !enable;
      $('arc-tos-accept-button').disabled = !enable;
      $('arc-tos-retry-button').disabled = !enable;
      $('arc-tos-md').arcTosButtonsDisabled = !enable;
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
      this.lastFocusedElement = document.activeElement;
      $('arc-learn-more-content').innerHTML = content;
      $('arc-tos-overlay-content-text').hidden = false;
      $('arc-tos-overlay-text').hidden = false;
      $('arc-tos-overlay-text-close').focus();
    },

    /**
     * Opens external URL in popup overlay.
     * @param {string} targetUrl URL to open.
     */
    showUrlOverlay: function(targetUrl) {
      this.lastFocusedElement = document.activeElement;
      $('arc-tos-overlay-webview').src = targetUrl;
      $('arc-tos-overlay-url').hidden = false;
      $('arc-tos-overlay-url-close').focus();
    },

    /**
     * Hides overlay dialog.
     */
    hideOverlay: function() {
      $('arc-tos-overlay-text').hidden = true;
      $('arc-tos-overlay-url').hidden = true;
      if (this.lastFocusedElement) {
        this.lastFocusedElement.focus();
        this.lastFocusedElement = null;
      }
    },

    /**
     * Reloads Play Store ToS.
     */
    reloadPlayStoreToS: function() {
      this.termsError = false;
      var termsView = this.getElement_('arc-tos-view');
      termsView.src = 'https://play.google.com/about/play-terms.html';
      this.removeClass_('arc-tos-loaded');
      this.removeClass_('error');
      this.addClass_('arc-tos-loading');
      this.enableButtons_(false);
    },

    /**
     * Adds new class to the list of classes of root OOBE MD style and legacy
     * style root elements.
     * @param {string} className class to remove.
     *
     * @private
     */
    addClass_: function(className) {
      this.classList.add(className);
      $('arc-tos-md').getElement('arc-tos-dialog-md').classList.add(className);
    },

    /**
     * Removes class from the list of classes of root OOBE MD style and legacy
     * style root elements.
     * @param {string} className class to remove.
     *
     * @private
     */
    removeClass_: function(className) {
      this.classList.remove(className);
      $('arc-tos-md')
          .getElement('arc-tos-dialog-md')
          .classList.remove(className);
    },

    /**
     * Handles event when terms view is loaded.
     */
    onTermsViewContentLoad: function() {
      if (this.termsError) {
        return;
      }

      this.removeClass_('arc-tos-loading');
      this.removeClass_('error');
      this.addClass_('arc-tos-loaded');

      this.enableButtons_(true);
      this.getElement_('arc-tos-accept-button').focus();

      var termsView = this.getElement_('arc-tos-view');
      var termsViewContainer = this.getElement_('arc-tos-view-container');
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
      this.removeClass_('arc-tos-loading');
      this.removeClass_('arc-tos-loaded');
      this.addClass_('error');

      this.enableButtons_(true);
      this.getElement_('arc-tos-retry-button').focus();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.setLearnMoreHandlers_();

      Oobe.getInstance().headerHidden = true;

      // Reload caption image in case it was not loaded during the
      // initialization phase.
      $('arc-tos-logo').src =
          'https://play.google.com/about/images/play_logo.png';

      this.hideOverlay();
    },

    /**
     * Returns requested element from related part of HTML determined by current
     * MD OOBE mode.
     * @param {string} id Id of an element to find.
     *
     * @private
     */
    getElement_: function(id) {
      if (this.useMDOobe) {
        return $('arc-tos-md').getElement(id + '-md');
      }
      return $(id);
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      this.setMDMode_();
      this.setLearnMoreHandlers_();

      // We might need to reload Play Store ToS in case language was changed.
      if (this.countryCode_) {
        this.loadPlayStoreToS(this.countryCode_);
      }
    },

    /**
     * Sets handlers for learn more links for backup and restore and location
     * service options.
     *
     * @private
     */
    setLearnMoreHandlers_: function() {
      var self = this;

      var leanMoreBackupAndRestoreText =
          loadTimeData.getString('arcLearnMoreBackupAndRestore');
      var backupAndRestore = this.getElement_('arc-enable-backup-restore');
      backupAndRestore.parentElement
          .querySelector('#learn-more-link-backup-restore')
          .onclick = function(event) {
        event.stopPropagation();
        self.showLearnMoreOverlay(leanMoreBackupAndRestoreText);
      };

      var leanMoreLocationServiceText =
          loadTimeData.getString('arcLearnMoreLocationService');
      var locationService = this.getElement_('arc-enable-location-service');
      locationService.parentElement
          .querySelector('#learn-more-link-location-service')
          .onclick = function(event) {
        event.stopPropagation();
        self.showLearnMoreOverlay(leanMoreLocationServiceText);
      };
    }
  };
});
