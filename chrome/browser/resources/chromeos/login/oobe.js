// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

var localStrings = new LocalStrings();


cr.define('cr.ui', function() {
  const SCREEN_SIGNIN = 'signin';
  const SCREEN_GAIA_SIGNIN = 'gaia-signin';
  const SCREEN_ACCOUNT_PICKER = 'account-picker';

  /* Accelerator identifiers. Must be kept in sync with webui_login_view.cc. */
  const ACCELERATOR_ACCESSIBILITY = 'accessibility';
  const ACCELERATOR_ENROLLMENT = 'enrollment';

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  *
  * @constructor
  */
  function Oobe() {
  }

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    /**
     * Registered screens.
     */
    screens_: [],

    /**
     * Current OOBE step, index in the screens array.
     * @type {number}
     */
    currentStep_: 0,

    /**
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    },

    /**
     * Handle accelerators.
     * @param {string} name Accelerator name.
     */
    handleAccelerator: function(name) {
      if (name == ACCELERATOR_ACCESSIBILITY) {
        chrome.send('toggleAccessibility', []);
      } else if (ACCELERATOR_ENROLLMENT) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_SIGNIN ||
            currentStepId == SCREEN_GAIA_SIGNIN) {
          chrome.send('toggleEnrollmentScreen', []);
        }
      }
    },

    /**
     * Appends buttons to the button strip.
     * @param {Array} buttons Array with the buttons to append.
     */
    appendButtons_ : function(buttons) {
      if (buttons) {
        var buttonStrip = $('button-strip');
        for (var i = 0; i < buttons.length; ++i) {
          var button = buttons[i];
          buttonStrip.appendChild(button);
        }
      }
    },

    /**
     * Updates a step's css classes to reflect left, current, or right position.
     * @param {number} stepIndex step index.
     * @param {string} state one of 'left', 'current', 'right'.
     */
    updateStep_: function(stepIndex, state) {
      var stepId = this.screens_[stepIndex];
      var step = $(stepId);
      var header = $('header-' + stepId);
      var states = [ 'left', 'right', 'current' ];
      for (var i = 0; i < states.length; ++i) {
        if (states[i] != state) {
          step.classList.remove(states[i]);
          header.classList.remove(states[i]);
        }
      }
      step.classList.add(state);
      header.classList.add(state);
    },

    /**
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex, screenData) {
      var currentStepId = this.screens_[this.currentStep_];
      var nextStepId = this.screens_[nextStepIndex];
      var oldStep = $(currentStepId);
      var newStep = $(nextStepId);
      var newHeader = $('header-' + nextStepId);

      if (newStep.onBeforeShow)
        newStep.onBeforeShow(screenData);

      newStep.classList.remove('hidden');

      if (Oobe.isOobeUI()) {
        // Start gliding animation for OOBE steps.
        if (nextStepIndex > this.currentStep_) {
          for (var i = this.currentStep_; i < nextStepIndex; ++i)
            this.updateStep_(i, 'left');
          this.updateStep_(nextStepIndex, 'current');
        } else if (nextStepIndex < this.currentStep_) {
          for (var i = this.currentStep_; i > nextStepIndex; --i)
            this.updateStep_(i, 'right');
          this.updateStep_(nextStepIndex, 'current');
        }
      } else {
        // Start fading animation for login display.
        oldStep.classList.add('faded');
        newStep.classList.remove('faded');
      }

      // Adjust inner container height based on new step's height.
      $('inner-container').style.height = newStep.offsetHeight + 'px';

      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        oldStep.addEventListener('webkitTransitionEnd', function f(e) {
          oldStep.removeEventListener('webkitTransitionEnd', f);
          oldStep.classList.add('hidden');
        });
      } else {
        // First screen on OOBE launch.
        newHeader.classList.remove('right');
      }
      this.currentStep_ = nextStepIndex;
      $('oobe').className = nextStepId;
    },

    /**
     * Show screen of given screen id.
     * @param {Object} screen Screen params dict,
     *                        e.g. {id: screenId, data: data}
     */
    showScreen: function(screen) {
      var screenId = screen.id;

      // Show sign-in screen instead of account picker if pod row is empty.
      if (screenId == SCREEN_ACCOUNT_PICKER && $('pod-row').pods.length == 0) {
        Oobe.showSigninUI();
        return;
      }

      var data = screen.data;
      var index = this.getScreenIndex_(screenId);
      if (index >= 0)
        this.toggleStep_(index, data);
      $('offline-message').update();
    },

    /**
     * Gets index of given screen id in screens_.
     * @param {string} screenId Id of the screen to look up.
     * @private
     */
    getScreenIndex_: function(screenId) {
      for (var i = 0; i < this.screens_.length; ++i) {
        if (this.screens_[i] == screenId)
          return i;
      }
      return -1;
    },

    /**
     * Register an oobe screen.
     * @param {Element} el Decorated screen element.
     */
    registerScreen: function(el) {
      var screenId = el.id;
      this.screens_.push(screenId);

      var header = document.createElement('span');
      header.id = 'header-' + screenId;
      header.className = 'header-section right';
      header.textContent = el.header ? el.header : '';
      $('header-sections').appendChild(header);

      var dot = document.createElement('div');
      dot.id = screenId + '-dot';
      dot.className = 'progdot';
      $('progress').appendChild(dot);

      this.appendButtons_(el.buttons);
    },

    /**
     * Updates headers and buttons of the screens.
     * Should be executed on language change.
     */
    updateHeadersAndButtons_: function() {
      $('button-strip').innerHTML = '';
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        $('header-' + screenId).textContent = screen.header;
        this.appendButtons_(screen.buttons);
      }
    },

    /**
     * Prepares screens to use in login display.
     */
    prepareForLoginDisplay_ : function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);

        screen.classList.add('faded');
        screen.classList.remove('right');
        screen.classList.remove('left');
      }
    }
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
      select.addEventListener('change', function(event) {
        chrome.send(callback, [select.options[select.selectedIndex].value]);
      });
    }
  }

  /**
   * Returns offset (top, left) of the element.
   * @param {!Element} element HTML element
   * @return {!Object} The offset (top, left).
   */
  Oobe.getOffset = function(element) {
    var x = 0;
    var y = 0;
    while(element && !isNaN(element.offsetLeft) && !isNaN(element.offsetTop)) {
      x += element.offsetLeft - element.scrollLeft;
      y += element.offsetTop - element.scrollTop;
      element = element.offsetParent;
    }
    return { top: y, left: x };
  };

  /**
   * Initializes the OOBE flow.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  Oobe.initialize = function() {
    oobe.NetworkScreen.register();
    oobe.EulaScreen.register();
    oobe.UpdateScreen.register();
    oobe.EnrollmentScreen.register();
    oobe.OAuthEnrollmentScreen.register();
    login.AccountPickerScreen.register();
    if (localStrings.getString('authType') == 'webui')
      login.SigninScreen.register();
    else
      login.GaiaSigninScreen.register();
    oobe.UserImageScreen.register();
    login.OfflineMessageScreen.register();

    cr.ui.Bubble.decorate($('bubble'));

    $('security-link').addEventListener('click', function(event) {
      chrome.send('eulaOnTpmPopupOpened', []);
      $('popup-overlay').hidden = false;
    });
    $('security-ok-button').addEventListener('click', function(event) {
      $('popup-overlay').hidden = true;
    });

    $('shutdown-button').addEventListener('click', function(e) {
      chrome.send('shutdownSystem');
    });
    $('add-user-button').addEventListener('click', function(e) {
      if (window.navigator.onLine) {
        Oobe.showSigninUI();
      } else {
        $('bubble').showTextForElement($('add-user-button'),
            localStrings.getString('addUserOfflineMessage'));
      }
    });
    $('cancel-add-user-button').addEventListener('click', function(e) {
      this.hidden = true;
      $('add-user-button').hidden = false;
      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
    });

    chrome.send('screenStateInitialize', []);
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
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showScreen = function(screen) {
    Oobe.getInstance().showScreen(screen);
  };

  /**
   * Enables/disables continue button.
   * @param {bool} enable Should the button be enabled?
   */
  Oobe.enableContinueButton = function(enable) {
    $('continue-button').disabled = !enable;
  };

  /**
   * Sets usage statistics checkbox.
   * @param {bool} checked Is the checkbox checked?
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
   * Sets update message, which is shown above the progress bar.
   * @param {text} message Message which is shown by the label.
   */
  Oobe.setUpdateMessage = function(message) {
    $('update-upper-label').textContent = message;
  };

  /**
   * Shows or hides update curtain.
   * @param {bool} enable Are curtains shown?
   */
  Oobe.showUpdateCurtain = function(enable) {
    $('update-screen-curtain').hidden = !enable;
    $('update-screen-main').hidden = enable;
  };

  /**
   * Sets TPM password.
   * @param {text} password TPM password to be shown.
   */
  Oobe.setTpmPassword = function(password) {
    $('tpm-busy').hidden = true;
    $('tpm-password').textContent = password;
    $('tpm-password').hidden = false;
  }

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

    // Update headers & buttons.
    Oobe.updateHeadersAndButtons();
  }

  /**
   * Updates headers and buttons of the screens.
   * Should be executed on language change.
   */
  Oobe.updateHeadersAndButtons = function() {
    Oobe.getInstance().updateHeadersAndButtons_();
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
    $('login-header-bar').hidden = showOobe;
  };

  /**
   * Returns true if Oobe UI is shown.
   */
  Oobe.isOobeUI = function() {
    return !document.body.classList.contains('login-display');
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    $('add-user-button').hidden = true;
    $('cancel-add-user-button').hidden = false;
    $('add-user-header-bar-item').hidden = $('pod-row').pods.length == 0;
    chrome.send('showAddUser', [opt_email]);
  };

  /**
   * Resets sign-in input fields.
   */
  Oobe.resetSigninUI = function() {
    var currentScreenId = Oobe.getInstance().currentScreen.id;

    if (localStrings.getString('authType') == 'webui')
      $(SCREEN_SIGNIN).reset(currentScreenId == SCREEN_SIGNIN);
    else
      $(SCREEN_GAIA_SIGNIN).reset(currentScreenId == SCREEN_GAIA_SIGNIN);

    $('pod-row').reset(currentScreenId == SCREEN_ACCOUNT_PICKER);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;
    var anchor = undefined;
    var anchorPos = undefined;
    if (currentScreenId == SCREEN_SIGNIN) {
      anchor = $('email');

      // Show email field so that bubble shows up at the right location.
      $(SCREEN_SIGNIN).reset(true);
    } else if (currentScreenId == SCREEN_GAIA_SIGNIN) {
      // Use anchorPos since we won't be able to get the input fields of Gaia.
      anchorPos = Oobe.getOffset(Oobe.getInstance().currentScreen);

      // Ideally, we should just use
      //   anchorPos = Oobe.getOffset($('signin-frame'));
      // to get a good anchor point. However, this always gives (0,0) on
      // the device.
      // TODO(xiyuan): Figure out why the above fails and get rid of this.
      if ($('createAccount').hidden && $('guestSignin').hidden)
        anchorPos.left += 150;  // (640 - 340) / 2

      // TODO(xiyuan): Find a reliable way to align with Gaia UI.
      anchorPos.left += 60;
      anchorPos.top += 105;
    } else if (currentScreenId == SCREEN_ACCOUNT_PICKER &&
               $('pod-row').activated) {
      const MAX_LOGIN_ATTEMMPTS_IN_POD = 3;
      if (loginAttempts > MAX_LOGIN_ATTEMMPTS_IN_POD) {
        Oobe.showSigninUI($('pod-row').activated.user.emailAddress);
        return;
      }

      anchor = $('pod-row').activated.mainInput;
    }
    if (!anchor && !anchorPos) {
      console.log('Warning: Failed to find anchor for error :' +
                  message);
      return;
    }

    var error = document.createElement('div');

    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    if (link) {
      messageDiv.classList.add('error-message-padding');

      var helpLink = document.createElement('a');
      helpLink.href = '#';
      helpLink.textContent = link;
      helpLink.onclick = function(e) {
        chrome.send('launchHelpApp', [helpId]);
      }
      error.appendChild(helpLink);
    }

    if (anchor)
      $('bubble').showContentForElement(anchor, error);
    else if (anchorPos)
      $('bubble').showContentAt(anchorPos.left, anchorPos.top, error);
  };

  /**
   * Clears error bubble.
   */
  Oobe.clearErrors = function() {
    $('bubble').hide();
  };

  /**
   * Handles login success notification.
   */
  Oobe.onLoginSuccess = function(username) {
    if (Oobe.getInstance().currentScreen.id == SCREEN_ACCOUNT_PICKER) {
      $('pod-row').startAuthenticatedAnimation();
    }
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  Oobe.setLabelText = function(labelId, labelText) {
    $(labelId).textContent = labelText;
  };

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

// Disable text selection.
document.onselectstart = function(e) {
  e.preventDefault();
}

// Disable dragging.
document.ondragstart = function(e) {
  e.preventDefault();
}

document.addEventListener('DOMContentLoaded', cr.ui.Oobe.initialize);
