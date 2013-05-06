// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

<include src="login_common.js"></include>
<include src="oobe_screen_eula.js"></include>
<include src="oobe_screen_network.js"></include>
<include src="oobe_screen_update.js"></include>

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Setups given "select" element using the list and adds callback.
     * @param {!Element} select Select object to be updated.
     * @param {!Object} list List of the options to be added.
     * @param {string} callback Callback name which should be send to Chrome or
     * an empty string if the event listener shouldn't be added.
     */
    setupSelect: function(select, list, callback) {
      select.options.length = 0;
      for (var i = 0; i < list.length; ++i) {
        var item = list[i];
        var option =
            new Option(item.title, item.value, item.selected, item.selected);
        select.appendChild(option);
      }
      if (callback) {
        var sendCallback = function() {
          chrome.send(callback, [select.options[select.selectedIndex].value]);
        };
        select.addEventListener('blur', sendCallback);
        select.addEventListener('click', sendCallback);
        select.addEventListener('keyup', function(event) {
          var keycodeInterested = [
            9,  // Tab
            13,  // Enter
            27,  // Escape
          ];
          if (keycodeInterested.indexOf(event.keyCode) >= 0)
            sendCallback();
        });
      }
    },

    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      cr.ui.login.DisplayManager.initialize();
      login.WrongHWIDScreen.register();
      login.NetworkScreen.register();
      login.EulaScreen.register();
      login.UpdateScreen.register();
      login.ResetScreen.register();
      login.AccountPickerScreen.register();
      login.GaiaSigninScreen.register();
      login.UserImageScreen.register(/* lazyInit= */ false);
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
      login.LocallyManagedUserCreationScreen.register();
      login.TermsOfServiceScreen.register();

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));

      Oobe.initializeA11yMenu();

      chrome.send('screenStateInitialize');
    },

    /**
     * Initializes OOBE accessibility menu.
     */
    initializeA11yMenu: function() {
      cr.ui.Bubble.decorate($('accessibility-menu'));
      $('connect-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);
      $('eula-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);
      $('update-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);

      $('high-contrast').addEventListener('click',
                                          Oobe.handleHighContrastClick);
      $('spoken-feedback').addEventListener('click',
                                            Oobe.handleSpokenFeedbackClick);
      $('screen-magnifier').addEventListener('click',
                                             Oobe.handleScreenMagnifierClick);

      // A11y menu should be accessible i.e. disable autohide on any
      // keydown or click inside menu.
      $('accessibility-menu').hideOnKeyPress = false;
      $('accessibility-menu').hideOnSelfClick = false;
    },

    /**
     * Accessibility link handler.
     */
    handleAccessbilityLinkClick: function(e) {
      /** @const */ var BUBBLE_OFFSET = 5;
      /** @const */ var BUBBLE_PADDING = 10;
      $('accessibility-menu').showForElement(e.target,
                                             cr.ui.Bubble.Attachment.BOTTOM,
                                             BUBBLE_OFFSET, BUBBLE_PADDING);
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.defaultControl) {
        $('accessibility-menu').elementToFocusOnHide =
          Oobe.getInstance().currentScreen.defaultControl;
      } else {
        // Update screen falls into this category. Since it doesn't have any
        // controls other than a11y link we don't want that link to receive
        // focus when screen is shown i.e. defaultControl is not defined.
        // Focus a11y link instead.
        $('accessibility-menu').elementToFocusOnHide = e.target;
      }
      e.stopPropagation();
    },

    /**
     * Spoken feedback checkbox handler.
     */
    handleSpokenFeedbackClick: function(e) {
      chrome.send('enableSpokenFeedback', [$('spoken-feedback').checked]);
      e.stopPropagation();
    },

    /**
     * High contrast mode checkbox handler.
     */
    handleHighContrastClick: function(e) {
      chrome.send('enableHighContrast', [$('high-contrast').checked]);
      e.stopPropagation();
    },

    /**
     * Screen magnifier checkbox handler.
     */
    handleScreenMagnifierClick: function(e) {
      chrome.send('enableScreenMagnifier', [$('screen-magnifier').checked]);
      e.stopPropagation();
    },

    /**
     * Enables/disables continue button.
     * @param {boolean} enable Should the button be enabled?
     */
    enableContinueButton: function(enable) {
      $('continue-button').disabled = !enable;
    },

    /**
     * Sets usage statistics checkbox.
     * @param {boolean} checked Is the checkbox checked?
     */
    setUsageStats: function(checked) {
      $('usage-stats').checked = checked;
    },

    /**
     * Set OEM EULA URL.
     * @param {text} oemEulaUrl OEM EULA URL.
     */
    setOemEulaUrl: function(oemEulaUrl) {
      if (oemEulaUrl) {
        $('oem-eula-frame').src = oemEulaUrl;
        $('eulas').classList.remove('one-column');
      } else {
        $('eulas').classList.add('one-column');
      }
    },

    /**
     * Sets update's progress bar value.
     * @param {number} progress Percentage of the progress bar.
     */
    setUpdateProgress: function(progress) {
      $('update-progress-bar').value = progress;
    },

    /**
     * Shows or hides downloading ETA message.
     * @param {boolean} visible Are ETA message visible?
     */
    showEstimatedTimeLeft: function(visible) {
      $('progress-message').hidden = visible;
      $('estimated-time-left').hidden = !visible;
    },

    /**
     * Sets estimated time left until download will complete.
     * @param {number} seconds Time left in seconds.
     */
    setEstimatedTimeLeft: function(seconds) {
      var minutes = Math.ceil(seconds / 60);
      var message = '';
      if (minutes > 60) {
        message = loadTimeData.getString('downloadingTimeLeftLong');
      } else if (minutes > 55) {
        message = loadTimeData.getString('downloadingTimeLeftStatusOneHour');
      } else if (minutes > 20) {
        message = loadTimeData.getStringF('downloadingTimeLeftStatusMinutes',
                                          Math.ceil(minutes / 5) * 5);
      } else if (minutes > 1) {
        message = loadTimeData.getStringF('downloadingTimeLeftStatusMinutes',
                                          minutes);
      } else {
        message = loadTimeData.getString('downloadingTimeLeftSmall');
      }
      $('estimated-time-left').textContent =
        loadTimeData.getStringF('downloading', message);
    },

    /**
     * Shows or hides info message below progress bar.
     * @param {boolean} visible Are message visible?
     */
    showProgressMessage: function(visible) {
      $('estimated-time-left').hidden = visible;
      $('progress-message').hidden = !visible;
    },

    /**
     * Sets message below progress bar.
     * @param {string} message Message that should be shown.
     */
    setProgressMessage: function(message) {
      $('progress-message').innerText = message;
    },

    /**
     * Sets update message, which is shown above the progress bar.
     * @param {text} message Message which is shown by the label.
     */
    setUpdateMessage: function(message) {
      $('update-upper-label').textContent = message;
    },

    /**
     * Shows or hides update curtain.
     * @param {boolean} visible Are curtains visible?
     */
    showUpdateCurtain: function(visible) {
      $('update-screen-curtain').hidden = !visible;
      $('update-screen-main').hidden = visible;
    },

    /**
     * Sets TPM password.
     * @param {text} password TPM password to be shown.
     */
    setTpmPassword: function(password) {
      $('tpm-busy').hidden = true;

      if (password.length) {
        $('tpm-password').textContent = password;
        $('tpm-password').hidden = false;
      } else {
        $('tpm-desc').hidden = true;
        $('tpm-desc-powerwash').hidden = false;
      }
    },

    /**
     * Refreshes a11y menu state.
     * @param {!Object} data New dictionary with a11y features state.
     */
    refreshA11yInfo: function(data) {
      $('high-contrast').checked = data.highContrastEnabled;
      $('spoken-feedback').checked = data.spokenFeedbackEnabled;
      $('screen-magnifier').checked = data.screenMagnifierEnabled;
    },

    /**
     * Reloads content of the page (localized strings, options of the select
     * controls).
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      // Reload global local strings, process DOM tree again.
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);

      // Update language and input method menu lists.
      Oobe.setupSelect($('language-select'), data.languageList, '');
      Oobe.setupSelect($('keyboard-select'), data.inputMethodsList, '');

      // Update localized content of the screens.
      Oobe.updateLocalizedContent();
    },

    /**
     * Updates localized content of the screens.
     * Should be executed on language change.
     */
    updateLocalizedContent: function() {
      // Buttons, headers and links.
      Oobe.getInstance().updateLocalizedContent_();
    }
  };
});
