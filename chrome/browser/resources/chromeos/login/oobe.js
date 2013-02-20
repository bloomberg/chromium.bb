// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

var localStrings = new LocalStrings();

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function Oobe() {
  }

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Setups given "select" element using the list and adds callback.
   * @param {!Element} select Select object to be updated.
   * @param {!Object} list List of the options to be added.
   * @param {string} callback Callback name which should be send to Chrome or
   * an empty string if the event listener shouldn't be added.
   */
  Oobe.setupSelect = function(select, list, callback) {
    select.options.length = 0;
    for (var i = 0; i < list.length; ++i) {
      var item = list[i];
      var option =
          new Option(item.title, item.value, item.selected, item.selected);
      select.appendChild(option);
    }
    if (callback) {
      var send_callback = function() {
        chrome.send(callback, [select.options[select.selectedIndex].value]);
      };
      select.addEventListener('blur', function(event) { send_callback(); });
      select.addEventListener('click', function(event) { send_callback(); });
      select.addEventListener('keyup', function(event) {
        var keycode_interested = [
          9,  // Tab
          13,  // Enter
          27,  // Escape
        ];
        if (keycode_interested.indexOf(event.keyCode) >= 0)
          send_callback();
      });
    }
  };

  /**
   * Initializes the OOBE flow.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  Oobe.initialize = function() {
    DisplayManager.initialize();
    oobe.WrongHWIDScreen.register();
    oobe.NetworkScreen.register();
    oobe.EulaScreen.register();
    oobe.UpdateScreen.register();
    oobe.OAuthEnrollmentScreen.register();
    oobe.ResetScreen.register();
    login.AccountPickerScreen.register();
    login.GaiaSigninScreen.register();
    oobe.UserImageScreen.register(/* lazyInit= */ false);
    login.ErrorMessageScreen.register();
    login.TPMErrorMessageScreen.register();
    login.PasswordChangedScreen.register();
    login.ManagedUserCreationScreen.register();
    oobe.TermsOfServiceScreen.register();

    cr.ui.Bubble.decorate($('bubble'));
    login.HeaderBar.decorate($('login-header-bar'));

    Oobe.initializeA11yMenu();

    chrome.send('screenStateInitialize');
  };

  /**
   * Initializes OOBE accessibility menu.
   */
  Oobe.initializeA11yMenu = function() {
    cr.ui.Bubble.decorate($('accessibility-menu'));
    $('connect-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);
    $('eula-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);
    $('update-accessibility-link').addEventListener(
        'click', Oobe.handleAccessbilityLinkClick);

    $('high-contrast').addEventListener('click', Oobe.handleHighContrastClick);
    $('spoken-feedback').addEventListener('click',
                                          Oobe.handleSpokenFeedbackClick);
    $('screen-magnifier').addEventListener('click',
                                           Oobe.handleScreenMagnifierClick);

    // A11y menu should be accessible i.e. disable autohide on any
    // keydown or click inside menu.
    $('accessibility-menu').hideOnKeyPress = false;
    $('accessibility-menu').hideOnSelfClick = false;
  };

  /**
   * Handle accelerators. These are passed from native code instead of a JS
   * event handler in order to make sure that embedded iframes cannot swallow
   * them.
   * @param {string} name Accelerator name.
   */
  Oobe.handleAccelerator = function(name) {
    Oobe.getInstance().handleAccelerator(name);
  };

  /**
   * Accessibility link handler.
   */
  Oobe.handleAccessbilityLinkClick = function(e) {
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
      // controls other than a11y link we don't want that link to receive focus
      // when screen is shown i.e. defaultControl is not defined.
      // Focus a11y link instead.
      $('accessibility-menu').elementToFocusOnHide = e.target;
    }
    e.stopPropagation();
  };

  /**
   * Spoken feedback checkbox handler.
   */
  Oobe.handleSpokenFeedbackClick = function(e) {
    chrome.send('enableSpokenFeedback', [$('spoken-feedback').checked]);
    e.stopPropagation();
  };

  /**
   * High contrast mode checkbox handler.
   */
  Oobe.handleHighContrastClick = function(e) {
    chrome.send('enableHighContrast', [$('high-contrast').checked]);
    e.stopPropagation();
  };

  /**
   * Screen magnifier checkbox handler.
   */
  Oobe.handleScreenMagnifierClick = function(e) {
    chrome.send('enableScreenMagnifier', [$('screen-magnifier').checked]);
    e.stopPropagation();
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showScreen = function(screen) {
    Oobe.getInstance().showScreen(screen);
  };

  /**
   * Shows the previous screen of workflow.
   */
  Oobe.goBack = function() {
    Oobe.getInstance().goBack();
  };

  /**
   * Enables/disables continue button.
   * @param {boolean} enable Should the button be enabled?
   */
  Oobe.enableContinueButton = function(enable) {
    $('continue-button').disabled = !enable;
  };

  /**
   * Sets usage statistics checkbox.
   * @param {boolean} checked Is the checkbox checked?
   */
  Oobe.setUsageStats = function(checked) {
    $('usage-stats').checked = checked;
  };

  /**
   * Set OEM EULA URL.
   * @param {text} oemEulaUrl OEM EULA URL.
   */
  Oobe.setOemEulaUrl = function(oemEulaUrl) {
    if (oemEulaUrl) {
      $('oem-eula-frame').src = oemEulaUrl;
      $('eulas').classList.remove('one-column');
    } else {
      $('eulas').classList.add('one-column');
    }
  };

  /**
   * Sets update's progress bar value.
   * @param {number} progress Percentage of the progress bar.
   */
  Oobe.setUpdateProgress = function(progress) {
    $('update-progress-bar').value = progress;
  };

  /**
   * Shows or hides downloading ETA message.
   * @param {boolean} visible Are ETA message visible?
   */
  Oobe.showEstimatedTimeLeft = function(visible) {
    $('progress-message').hidden = visible;
    $('estimated-time-left').hidden = !visible;
  };

  /**
   * Sets estimated time left until download will complete.
   * @param {number} seconds Time left in seconds.
   */
  Oobe.setEstimatedTimeLeft = function(seconds) {
    var minutes = Math.ceil(seconds / 60);
    var message = '';
    if (minutes > 60) {
      message = localStrings.getString('downloadingTimeLeftLong');
    } else if (minutes > 55) {
      message = localStrings.getString('downloadingTimeLeftStatusOneHour');
    } else if (minutes > 20) {
      message = localStrings.getStringF('downloadingTimeLeftStatusMinutes',
                                        Math.ceil(minutes / 5) * 5);
    } else if (minutes > 1) {
      message = localStrings.getStringF('downloadingTimeLeftStatusMinutes',
                                        minutes);
    } else {
      message = localStrings.getString('downloadingTimeLeftSmall');
    }
    $('estimated-time-left').textContent =
      localStrings.getStringF('downloading', message);
  };

  /**
   * Shows or hides info message below progress bar.
   * @param {boolean} visible Are message visible?
   */
  Oobe.showProgressMessage = function(visible) {
    $('estimated-time-left').hidden = visible;
    $('progress-message').hidden = !visible;
  };

  /**
   * Sets message below progress bar.
   * @param {string} message Message that should be shown.
   */
  Oobe.setProgressMessage = function(message) {
    $('progress-message').innerText = message;
  };

  /**
   * Sets update message, which is shown above the progress bar.
   * @param {text} message Message which is shown by the label.
   */
  Oobe.setUpdateMessage = function(message) {
    $('update-upper-label').textContent = message;
  };

  /**
   * Shows or hides update curtain.
   * @param {boolean} visible Are curtains visible?
   */
  Oobe.showUpdateCurtain = function(visible) {
    $('update-screen-curtain').hidden = !visible;
    $('update-screen-main').hidden = visible;
  };

  /**
   * Sets TPM password.
   * @param {text} password TPM password to be shown.
   */
  Oobe.setTpmPassword = function(password) {
    $('tpm-busy').hidden = true;

    if (password.length) {
      $('tpm-password').textContent = password;
      $('tpm-password').hidden = false;
    } else {
      $('tpm-desc').hidden = true;
      $('tpm-desc-powerwash').hidden = false;
    }
  }

  /**
   * Refreshes a11y menu state.
   * @param {!Object} data New dictionary with a11y features state.
   */
  Oobe.refreshA11yInfo = function(data) {
    $('high-contrast').checked = data.highContrastEnabled;
    $('spoken-feedback').checked = data.spokenFeedbackEnabled;
    $('screen-magnifier').checked = data.screenMagnifierEnabled;
  };

  /**
   * Reloads content of the page (localized strings, options of the select
   * controls).
   * @param {!Object} data New dictionary with i18n values.
   */
  Oobe.reloadContent = function(data) {
    // Reload global local strings, process DOM tree again.
    templateData = data;
    i18nTemplate.process(document, data);

    // Update language and input method menu lists.
    Oobe.setupSelect($('language-select'), data.languageList, '');
    Oobe.setupSelect($('keyboard-select'), data.inputMethodsList, '');

    // Update localized content of the screens.
    Oobe.updateLocalizedContent();
  };

  /**
   * Updates version label visibilty.
   * @param {boolean} show True if version label should be visible.
   */
  Oobe.showVersion = function(show) {
    Oobe.getInstance().showVersion(show);
  };

  /**
   * Updates localized content of the screens.
   * Should be executed on language change.
   */
  Oobe.updateLocalizedContent = function() {
    // Buttons, headers and links.
    Oobe.getInstance().updateLocalizedContent_();
  };

  /**
   * Update body class to switch between OOBE UI and Login UI.
   */
  Oobe.showOobeUI = function(showOobe) {
    if (showOobe) {
      document.body.classList.remove('login-display');
    } else {
      document.body.classList.add('login-display');
      Oobe.getInstance().prepareForLoginDisplay_();
    }

    // Don't show header bar for OOBE.
    Oobe.getInstance().headerHidden = showOobe;
  };

  /**
   * Disables signin UI.
   */
  Oobe.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  Oobe.resetSigninUI = function(forceOnline) {
    DisplayManager.resetSigninUI(forceOnline);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Shows password changed screen that offers migration.
   * @param {boolean} showError Whether to show the incorrect password error.
   */
  Oobe.showPasswordChangedScreen = function(showError) {
    DisplayManager.showPasswordChangedScreen(showError);
  };

  /**
   * Shows dialog to create managed user.
   */
  Oobe.showManagedUserCreationScreen = function() {
    DisplayManager.showManagedUserCreationScreen();
  };

  /**
   * Shows TPM error screen.
   */
  Oobe.showTpmError = function() {
    DisplayManager.showTpmError();
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  Oobe.clearErrors = function() {
    $('accessibility-menu').hide();
    DisplayManager.clearErrors();
  };

  /**
   * Displays animations on successful authentication, that have to happen
   * before login UI is dismissed.
   */
  Oobe.animateAuthenticationSuccess = function() {
    login.HeaderBar.animateOut(function() {
      chrome.send('unlockOnLoginSuccess');
    });
  };

  /**
   * Displays animations that have to happen once login UI is fully displayed.
   */
  Oobe.animateOnceFullyDisplayed = function() {
    login.HeaderBar.animateIn();
  };

  /**
   * Handles login success notification.
   */
  Oobe.onLoginSuccess = function(username) {
    if (Oobe.getInstance().currentScreen.id == SCREEN_ACCOUNT_PICKER) {
      // TODO(nkostylev): Enable animation back when session start jank
      // is reduced. See http://crosbug.com/11116 http://crosbug.com/18307
      // $('pod-row').startAuthenticatedAnimation();
    }
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  Oobe.setLabelText = function(labelId, labelText) {
    DisplayManager.setLabelText(labelId, labelText);
  };

  /**
   * Sets the text content of the enterprise info message.
   * If the text is empty, the entire notification will be hidden.
   * @param {string} messageText The message text.
   */
  Oobe.setEnterpriseInfo = function(messageText) {
    DisplayManager.setEnterpriseInfo(messageText);
  };

  /**
   * Enforces focus on user pod of locked user.
   */
  Oobe.forceLockedUserPodFocus = function() {
    login.AccountPickerScreen.forceLockedUserPodFocus();
  };

  /**
   * Sets the domain name whose Terms of Service are being shown on the Terms of
   * Service screen.
   * @param {string} domain The domain name.
   */
  Oobe.setTermsOfServiceDomain = function(domain) {
    oobe.TermsOfServiceScreen.setDomain(domain);
  };

  /**
   * Displays an error message on the Terms of Service screen. Called when the
   * download of the Terms of Service has failed.
   */
  Oobe.setTermsOfServiceLoadError = function() {
    $('terms-of-service').classList.remove('tos-loading');
    $('terms-of-service').classList.add('error');
  };

  /**
   * Displays the given |termsOfService| on the Terms of Service screen.
   * @param {string} termsOfService The terms of service, as plain text.
   */
  Oobe.setTermsOfService = function(termsOfService) {
    oobe.TermsOfServiceScreen.setTermsOfService(termsOfService);
  };

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', cr.ui.Oobe.initialize);
