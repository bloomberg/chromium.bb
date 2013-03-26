// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

// TODO(xiyuan): Find a better to share those constants.
/** @const */ var SCREEN_OOBE_NETWORK = 'connect';
/** @const */ var SCREEN_OOBE_EULA = 'eula';
/** @const */ var SCREEN_OOBE_UPDATE = 'update';
/** @const */ var SCREEN_OOBE_ENROLLMENT = 'oauth-enrollment';
/** @const */ var SCREEN_GAIA_SIGNIN = 'gaia-signin';
/** @const */ var SCREEN_ACCOUNT_PICKER = 'account-picker';
/** @const */ var SCREEN_ERROR_MESSAGE = 'error-message';
/** @const */ var SCREEN_USER_IMAGE_PICKER = 'user-image';
/** @const */ var SCREEN_TPM_ERROR = 'tpm-error-message';
/** @const */ var SCREEN_PASSWORD_CHANGED = 'password-changed';
/** @const */ var SCREEN_CREATE_MANAGED_USER_DIALOG =
    'managed-user-creation-dialog';
/** @const */ var SCREEN_CREATE_MANAGED_USER_FLOW =
    'managed-user-creation-flow';

/* Accelerator identifiers. Must be kept in sync with webui_login_view.cc. */
/** @const */ var ACCELERATOR_CANCEL = 'cancel';
/** @const */ var ACCELERATOR_ENROLLMENT = 'enrollment';
/** @const */ var ACCELERATOR_VERSION = 'version';
/** @const */ var ACCELERATOR_RESET = 'reset';

/* Help topic identifiers. */
/** @const */ var HELP_TOPIC_ENTERPRISE_REPORTING = 2535613;

/* Signin UI state constants. Used to control header bar UI. */
/** @const */ var SIGNIN_UI_STATE = {
  HIDDEN: 0,
  GAIA_SIGNIN: 1,
  ACCOUNT_PICKER: 2,
  WRONG_HWID_WARNING: 3,
  MANAGED_USER_CREATION_DIALOG: 4,
  MANAGED_USER_CREATION_FLOW: 5,
};

cr.define('cr.ui.login', function() {
  var Bubble = cr.ui.Bubble;

  /**
   * Groups of screens (screen IDs) that should have the same dimensions.
   * @type Array.<Array.<string>>
   * @const
   */
  var SCREEN_GROUPS = [[SCREEN_OOBE_NETWORK,
                        SCREEN_OOBE_EULA,
                        SCREEN_OOBE_UPDATE]
                      ];

  /**
   * Constructor a display manager that manages initialization of screens,
   * transitions, error messages display.
   *
   * @constructor
   */
  function DisplayManager() {
  }

  DisplayManager.prototype = {
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
     * Whether version label can be toggled by ACCELERATOR_VERSION.
     * @type {boolean}
     */
    allowToggleVersion_: false,

    /**
     * List of parameters to showScreen calls.
     * @type {array}
     */
    screenParametersHistory_: [],

    /**
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    },

    /**
     * Hides/shows header (Shutdown/Add User/Cancel buttons).
     * @param {boolean} hidden Whether header is hidden.
     */
    set headerHidden(hidden) {
      $('login-header-bar').hidden = hidden;
    },

    /**
     * Shows/hides version labels.
     * @param {boolean} show Whether labels should be visible by default. If
     *     false, visibility can be toggled by ACCELERATOR_VERSION.
     */
    showVersion: function(show) {
      $('version-labels').hidden = !show;
      this.allowToggleVersion_ = !show;
    },

    /**
     * Handle accelerators.
     * @param {string} name Accelerator name.
     */
    handleAccelerator: function(name) {
      if (name == ACCELERATOR_CANCEL) {
        if (this.currentScreen.cancel) {
          this.currentScreen.cancel();
        }
      } else if (name == ACCELERATOR_ENROLLMENT) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_GAIA_SIGNIN ||
            currentStepId == SCREEN_ACCOUNT_PICKER) {
          chrome.send('toggleEnrollmentScreen');
        } else if (currentStepId == SCREEN_OOBE_NETWORK ||
                   currentStepId == SCREEN_OOBE_EULA) {
          // In this case update check will be skipped and OOBE will
          // proceed straight to enrollment screen when EULA is accepted.
          chrome.send('skipUpdateEnrollAfterEula');
        } else if (currentStepId == SCREEN_OOBE_ENROLLMENT) {
          // This accelerator is also used to manually cancel auto-enrollment.
          if (this.currentScreen.cancelAutoEnrollment)
            this.currentScreen.cancelAutoEnrollment();
        }
      } else if (name == ACCELERATOR_VERSION) {
        if (this.allowToggleVersion_)
          $('version-labels').hidden = !$('version-labels').hidden;
      } else if (name == ACCELERATOR_RESET) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_GAIA_SIGNIN ||
            currentStepId == SCREEN_ACCOUNT_PICKER) {
          chrome.send('toggleResetScreen');
        }
      }
    },

    /**
     * Appends buttons to the button strip.
     * @param {Array.<HTMLElement>} buttons Array with the buttons to append.
     * @param {string} screenId Id of the screen that buttons belong to.
     */
    appendButtons_: function(buttons, screenId) {
      if (buttons) {
        var buttonStrip = $(screenId + '-controls');
        if (buttonStrip) {
          for (var i = 0; i < buttons.length; ++i)
            buttonStrip.appendChild(buttons[i]);
        }
      }
    },

    /**
     * Disables or enables control buttons on the specified screen.
     * @param {HTMLElement} screen Screen which controls should be affected.
     * @param {boolean} disabled Whether to disable controls.
     */
    disableButtons_: function(screen, disabled) {
      var buttons = document.querySelectorAll(
          '#' + screen.id + '-controls button:not(.preserve-disabled-state)');
      for (var i = 0; i < buttons.length; ++i) {
        buttons[i].disabled = disabled;
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
      var states = ['left', 'right', 'current'];
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

      // Disable controls before starting animation.
      this.disableButtons_(oldStep, true);

      if (oldStep.onBeforeHide)
        oldStep.onBeforeHide();

      if (newStep.onBeforeShow)
        newStep.onBeforeShow(screenData);

      newStep.classList.remove('hidden');

      if (newStep.onAfterShow)
        newStep.onAfterShow(screenData);

      this.disableButtons_(newStep, false);

      // Default control to be focused (if specified).
      var defaultControl = newStep.defaultControl;

      if (this.isOobeUI()) {
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
      this.updateScreenSize(newStep);

      var innerContainer = $('inner-container');
      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        if (oldStep.classList.contains('animated')) {
          innerContainer.classList.add('animation');
          oldStep.addEventListener('webkitTransitionEnd', function f(e) {
            oldStep.removeEventListener('webkitTransitionEnd', f);
            if (oldStep.classList.contains('faded') ||
                oldStep.classList.contains('left') ||
                oldStep.classList.contains('right')) {
              innerContainer.classList.remove('animation');
              oldStep.classList.add('hidden');
            }
            if (defaultControl)
              defaultControl.focus();
          });
        } else {
          oldStep.classList.add('hidden');
          if (defaultControl)
            defaultControl.focus();
        }
      } else {
        // First screen on OOBE launch.
        if (innerContainer.classList.contains('down')) {
          innerContainer.classList.remove('down');
          innerContainer.addEventListener(
              'webkitTransitionEnd', function f(e) {
                innerContainer.removeEventListener('webkitTransitionEnd', f);
                $('progress-dots').classList.remove('down');
                chrome.send('loginVisible', ['oobe']);
                if (defaultControl)
                  defaultControl.focus();
              });
        } else {
          if (defaultControl)
            defaultControl.focus();
        }
        newHeader.classList.remove('right');  // Old OOBE.
      }
      this.currentStep_ = nextStepIndex;
      $('oobe').className = nextStepId;

      $('step-logo').hidden = newStep.classList.contains('no-logo');

      chrome.send('updateCurrentScreen', [this.currentScreen.id]);
    },

    /**
     * Make sure that screen is initialized and decorated.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    preloadScreen: function(screen) {
      var screenEl = $(screen.id);
      if (screenEl.deferredDecorate !== undefined) {
        screenEl.deferredDecorate();
        delete screenEl.deferredDecorate;
      }
    },

    /**
     * Show screen of given screen id.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    showScreen: function(screen) {
      var screenId = screen.id;

      // As for now, support "back" only for create managed user screen.
      if (screenId != SCREEN_CREATE_MANAGED_USER_DIALOG) {
        this.screenParametersHistory_ = [];
      }

      this.screenParametersHistory_.push(screen);

      // Make sure the screen is decorated.
      this.preloadScreen(screen);

      if (screen.data !== undefined && screen.data.disableAddUser)
        DisplayManager.updateAddUserButtonStatus(true);


      // Show sign-in screen instead of account picker if pod row is empty.
      if (screenId == SCREEN_ACCOUNT_PICKER && $('pod-row').pods.length == 0) {
        // Manually hide 'add-user' header bar, because of the case when
        // 'Cancel' button is used on the offline login page.
        $('add-user-header-bar-item').hidden = true;
        Oobe.showSigninUI(true);
        return;
      }

      var data = screen.data;
      var index = this.getScreenIndex_(screenId);
      if (index >= 0)
        this.toggleStep_(index, data);
    },

    /**
     * Shows the previous screen of workflow.
     */
    goBack: function() {
      if (this.screenParametersHistory_.length >= 2) {
        this.screenParametersHistory_.pop();
        this.showScreen(this.screenParametersHistory_.pop());
      }
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
      header.textContent = el.header ? el.header : '';
      header.className = 'header-section';
      $('header-sections').appendChild(header);

      var dot = document.createElement('div');
      dot.id = screenId + '-dot';
      dot.className = 'progdot';
      $('progress-dots').appendChild(dot);

      this.appendButtons_(el.buttons, screenId);
    },

    /**
     * Updates inner container size based on the size of the current screen and
     * other screens in the same group.
     * Should be executed on screen change / screen size change.
     * @param {!HTMLElement} screen Screen that is being shown.
     */
    updateScreenSize: function(screen) {
      // Have to reset any previously predefined screen size first
      // so that screen contents would define it instead (offsetHeight/width).
      // http://crbug.com/146539
      screen.style.width = '';
      screen.style.height = '';

      var height = screen.offsetHeight;
      var width = screen.offsetWidth;
      for (var i = 0, screenGroup; screenGroup = SCREEN_GROUPS[i]; i++) {
        if (screenGroup.indexOf(screen.id) != -1) {
          // Set screen dimensions to maximum dimensions within this group.
          for (var j = 0, screen2; screen2 = $(screenGroup[j]); j++) {
            height = Math.max(height, screen2.offsetHeight);
            width = Math.max(width, screen2.offsetWidth);
          }
          break;
        }
      }
      $('inner-container').style.height = height + 'px';
      $('inner-container').style.width = width + 'px';
      // This requires |screen| to have 'box-sizing: border-box'.
      screen.style.width = width + 'px';
      screen.style.height = height + 'px';
    },

    /**
     * Updates localized content of the screens like headers, buttons and links.
     * Should be executed on language change.
     */
    updateLocalizedContent_: function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        var buttonStrip = $(screenId + '-controls');
        if (buttonStrip)
          buttonStrip.innerHTML = '';
        // TODO(nkostylev): Update screen headers for new OOBE design.
        this.appendButtons_(screen.buttons, screenId);
        if (screen.updateLocalizedContent)
          screen.updateLocalizedContent();
      }

      var currentScreenId = this.screens_[this.currentStep_];
      var currentScreen = $(currentScreenId);
      this.updateScreenSize(currentScreen);

      // Trigger network drop-down to reload its state
      // so that strings are reloaded.
      // Will be reloaded if drowdown is actually shown.
      cr.ui.DropDown.refresh();
    },

    /**
     * Prepares screens to use in login display.
     */
    prepareForLoginDisplay_: function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        screen.classList.add('faded');
        screen.classList.remove('right');
        screen.classList.remove('left');
      }
    },

    /**
     * Returns true if Oobe UI is shown.
     */
    isOobeUI: function() {
      return !document.body.classList.contains('login-display');
    },

    /**
     * Returns true if the current screen is the lock screen.
     */
    isLockScreen: function() {
      return document.documentElement.getAttribute('screen') == 'lock';
    },

    /**
     * Returns true if sign in UI should trigger wallpaper load on boot.
     */
    shouldLoadWallpaperOnBoot: function() {
      return loadTimeData.getString('bootIntoWallpaper') == 'on';
    },
  };

  /**
   * Initializes display manager.
   */
  DisplayManager.initialize = function() {
    var link = $('enterprise-info-hint-link');
    link.addEventListener(
        'click', DisplayManager.handleEnterpriseHintLinkClick);
  },

  /**
   * Returns offset (top, left) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} The offset (top, left).
   */
  DisplayManager.getOffset = function(element) {
    var x = 0;
    var y = 0;
    while (element && !isNaN(element.offsetLeft) && !isNaN(element.offsetTop)) {
      x += element.offsetLeft - element.scrollLeft;
      y += element.offsetTop - element.scrollTop;
      element = element.offsetParent;
    }
    return { top: y, left: x };
  };

  /**
   * Returns position (top, left, right, bottom) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} Element position (top, left, right, bottom).
   */
  DisplayManager.getPosition = function(element) {
    var offset = DisplayManager.getOffset(element);
    return { top: offset.top,
             right: window.innerWidth - element.offsetWidth - offset.left,
             bottom: window.innerHeight - element.offsetHeight - offset.top,
             left: offset.left };
  };

  /**
   * Disables signin UI.
   */
  DisplayManager.disableSigninUI = function() {
    $('login-header-bar').disabled = true;
    $('pod-row').disabled = true;
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  DisplayManager.showSigninUI = function(opt_email) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;
    if (currentScreenId == SCREEN_GAIA_SIGNIN)
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.GAIA_SIGNIN;
    else if (currentScreenId == SCREEN_ACCOUNT_PICKER)
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
    else if (currentScreenId == SCREEN_CREATE_MANAGED_USER_DIALOG)
      $('login-header-bar').signinUIState =
          SIGNIN_UI_STATE.MANAGED_USER_CREATION_DIALOG;
    chrome.send('showAddUser', [opt_email]);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   *     If |forceOnline| is false previously used sign-in type will be used.
   */
  DisplayManager.resetSigninUI = function(forceOnline) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;

    $(SCREEN_GAIA_SIGNIN).reset(
        currentScreenId == SCREEN_GAIA_SIGNIN, forceOnline);
    $('login-header-bar').disabled = false;
    $('pod-row').reset(currentScreenId == SCREEN_ACCOUNT_PICKER);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  DisplayManager.showSignInError = function(loginAttempts, message, link,
                                            helpId) {
    var error = document.createElement('div');

    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message-bubble';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    if (link) {
      messageDiv.classList.add('error-message-bubble-padding');

      var helpLink = document.createElement('a');
      helpLink.href = '#';
      helpLink.textContent = link;
      helpLink.addEventListener('click', function(e) {
        chrome.send('launchHelpApp', [helpId]);
        e.preventDefault();
      });
      error.appendChild(helpLink);
    }

    var currentScreenId = Oobe.getInstance().currentScreen.id;
    $(currentScreenId).showErrorBubble(loginAttempts, error);
  };

  /**
   * Shows password changed screen that offers migration.
   * @param {boolean} showError Whether to show the incorrect password error.
   */
  DisplayManager.showPasswordChangedScreen = function(showError) {
    login.PasswordChangedScreen.show(showError);
  };

  /**
   * Shows dialog to create managed user.
   */
  DisplayManager.showManagedUserCreationScreen = function() {
    login.ManagedUserCreationScreen.show();
  };

  /**
   * Shows TPM error screen.
   */
  DisplayManager.showTpmError = function() {
    login.TPMErrorMessageScreen.show();
  };

  /**
   * Clears error bubble.
   */
  DisplayManager.clearErrors = function() {
    $('bubble').hide();
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  DisplayManager.setLabelText = function(labelId, labelText) {
    $(labelId).textContent = labelText;
  };

  /**
   * Shows help topic about enrolled devices.
   * @param {MouseEvent} Event object.
   */
  DisplayManager.handleEnterpriseHintLinkClick = function(e) {
    chrome.send('launchHelpApp', [HELP_TOPIC_ENTERPRISE_REPORTING]);
    e.preventDefault();
  }

  /**
   * Sets the text content of the enterprise info message.
   * @param {string} messageText The message text.
   */
  DisplayManager.setEnterpriseInfo = function(messageText) {
    $('enterprise-info-message').textContent = messageText;
    if (messageText) {
      $('enterprise-info').hidden = false;
    }
  };

  /**
   * Disable Add users button if said.
   * @param {boolean} disable true to disable
   */
  DisplayManager.updateAddUserButtonStatus = function(disable) {
    $('add-user-button').disabled = disable;
    $('add-user-button').classList[
        disable ? 'add' : 'remove']('button-restricted');
    $('add-user-button').title = disable ?
        loadTimeData.getString('disabledAddUserTooltip') : '';
  }

  /**
   * Clears password field in user-pod.
   */
  DisplayManager.clearUserPodPassword = function() {
    $('pod-row').clearFocusedPod();
  };

  // Export
  return {
    DisplayManager: DisplayManager
  };
});
