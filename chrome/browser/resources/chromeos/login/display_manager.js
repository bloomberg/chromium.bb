// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

// TODO(xiyuan): Find a better to share those constants.
const SCREEN_SIGNIN = 'signin';
const SCREEN_GAIA_SIGNIN = 'gaia-signin';
const SCREEN_ACCOUNT_PICKER = 'account-picker';

/* Accelerator identifiers. Must be kept in sync with webui_login_view.cc. */
const ACCELERATOR_ACCESSIBILITY = 'accessibility';
const ACCELERATOR_CANCEL = 'cancel';
const ACCELERATOR_ENROLLMENT = 'enrollment';

cr.define('cr.ui.login', function() {
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
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    },

    /**
     * Hides/shows header (Shutdown/Add User/Cancel buttons).
     * @param {bool} hidden Whether header is hidden.
     */
    set headerHidden(hidden) {
      $('login-header-bar').hidden = hidden;
    },

    /**
     * Handle accelerators.
     * @param {string} name Accelerator name.
     */
    handleAccelerator: function(name) {
      if (name == ACCELERATOR_ACCESSIBILITY) {
        chrome.send('toggleAccessibility', []);
      } else if (name == ACCELERATOR_CANCEL) {
        if (this.currentScreen.cancel) {
          this.currentScreen.cancel();
        }
      } else if (name == ACCELERATOR_ENROLLMENT) {
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

      if (oldStep.onBeforeHide)
        oldStep.onBeforeHide();

      if (newStep.onBeforeShow)
        newStep.onBeforeShow(screenData);

      newStep.classList.remove('hidden');

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
      $('inner-container').style.height = newStep.offsetHeight + 'px';

      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        oldStep.addEventListener('webkitTransitionEnd', function f(e) {
          oldStep.removeEventListener('webkitTransitionEnd', f);
          if (oldStep.classList.contains('faded') ||
              oldStep.classList.contains('left') ||
              oldStep.classList.contains('right')) {
            oldStep.classList.add('hidden');
          }
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
      $('error-message').update();
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
     * Updates localized content of the screens like headers, buttons and links.
     * Should be executed on language change.
     */
    updateLocalizedContent_: function() {
      $('button-strip').innerHTML = '';
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        $('header-' + screenId).textContent = screen.header;
        this.appendButtons_(screen.buttons);
        if (screen.updateLocalizedContent)
          screen.updateLocalizedContent();
      }

      // This screen is a special case as it's not registered with the rest of
      // the screens.
      login.ErrorMessageScreen.updateLocalizedContent();

      // Trigger network drop-down to reload its state
      // so that strings are reloaded.
      // Will be reloaded if drowdown is actually shown.
      cr.ui.DropDown.refresh();
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
    },

    /**
     * Returns true if Oobe UI is shown.
     */
    isOobeUI: function() {
      return !document.body.classList.contains('login-display');
    }
  };

  /**
   * Returns offset (top, left) of the element.
   * @param {!Element} element HTML element
   * @return {!Object} The offset (top, left).
   */
  DisplayManager.getOffset = function(element) {
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
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  DisplayManager.showSigninUI = function(opt_email) {
    $('add-user-button').hidden = true;
    $('cancel-add-user-button').hidden = false;
    chrome.send('showAddUser', [opt_email]);
  };

  /**
   * Resets sign-in input fields.
   */
  DisplayManager.resetSigninUI = function() {
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
  DisplayManager.showSignInError = function(loginAttempts, message, link,
                                            helpId) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;
    var anchor = undefined;
    var anchorPos = undefined;
    if (currentScreenId == SCREEN_SIGNIN) {
      anchor = $('email');

      // Show email field so that bubble shows up at the right location.
      $(SCREEN_SIGNIN).reset(true);
    } else if (currentScreenId == SCREEN_GAIA_SIGNIN) {
      // Use anchorPos since we won't be able to get the input fields of Gaia.
      anchorPos = DisplayManager.getOffset(Oobe.getInstance().currentScreen);

      // Ideally, we should just use
      //   anchorPos = DisplayManager.getOffset($('signin-frame'));
      // to get a good anchor point. However, this always gives (0,0) on
      // the device.
      // TODO(xiyuan): Figure out why the above fails and get rid of this.
      anchorPos.left += 150;  // (640 - 340) / 2

      // TODO(xiyuan): Find a reliable way to align with Gaia UI.
      anchorPos.left += 60;
      anchorPos.top += 105;
    } else if (currentScreenId == SCREEN_ACCOUNT_PICKER &&
               $('pod-row').activatedPod) {
      const MAX_LOGIN_ATTEMMPTS_IN_POD = 3;
      if (loginAttempts > MAX_LOGIN_ATTEMMPTS_IN_POD) {
        $('pod-row').activatedPod.showSigninUI();
        return;
      }

      anchor = $('pod-row').activatedPod.mainInput;
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
      };
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

  // Export
  return {
    DisplayManager: DisplayManager
  };
});
