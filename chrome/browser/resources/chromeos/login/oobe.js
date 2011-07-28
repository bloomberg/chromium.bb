// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

var localStrings = new LocalStrings();


cr.define('cr.ui', function() {
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

    oobeKeyDown: function(e) {
      var keystroke = String.fromCharCode(e.keyCode);
      switch (keystroke) {
        case 'Z':
          if (e.altKey && e.ctrlKey) {
            chrome.send('toggleAccessibility', []);
            break;
          }
        case 'E':
          var currentStepId = this.screens_[this.currentStep_];
          if (currentStepId == 'signin') {
            if (e.altKey && e.ctrlKey) {
              chrome.send('toggleEnrollmentScreen', []);
              break;
            }
          }
      } // switch
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
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex, screenData) {
      var currentStepId = this.screens_[this.currentStep_];
      var nextStepId = this.screens_[nextStepIndex];
      var oldStep = $(currentStepId);
      var oldHeader = $('header-' + currentStepId);
      var newStep = $(nextStepId);
      var newHeader = $('header-' + nextStepId);

      if (newStep.onBeforeShow)
        newStep.onBeforeShow(screenData);

      newStep.classList.remove('hidden');

      if (Oobe.isOobeUI()) {
        // Start gliding animation for OOBE steps.
        if (nextStepIndex > this.currentStep_) {
          oldHeader.classList.add('left');
          oldStep.classList.add('left');
          newHeader.classList.remove('right');
          newStep.classList.remove('right');
        } else if (nextStepIndex < this.currentStep_) {
          oldHeader.classList.add('right');
          oldStep.classList.add('right');
          newHeader.classList.remove('left');
          newStep.classList.remove('left');
        }
      } else {
        // Start fading animation for login display.
        oldStep.classList.add('faded');
        newStep.classList.remove('faded');
      }

      // Adjust inner container height based on new step's height.
      $('inner-container').style.height = newStep.offsetHeight + 'px';

      if (this.currentStep_ != nextStepIndex) {
        oldStep.addEventListener('webkitTransitionEnd', function f(e) {
          oldStep.removeEventListener('webkitTransitionEnd', f);
          oldStep.classList.add('hidden');
          if (nextStepIndex == 0)
            Oobe.refreshNetworkControl();
        });
      } else {
        // First screen on OOBE launch.
        newHeader.classList.remove('right');
        if (nextStepIndex == 0) {
          Oobe.refreshNetworkControl();
        }
      }
      this.currentStep_ = nextStepIndex;
      $('oobe').className = nextStepId;
    },

    /**
     * Show screen of given screen id.
     * @param {string} screenId Id of the screen to show.
     */
    showScreen: function(screen) {
      var screenId = screen.id;
      var data = screen.data;
      var index = this.getScreenIndex_(screenId);
      if (index >= 0)
        this.toggleStep_(index, data);
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
    login.AccountPickerScreen.register();
    if (localStrings.getString('authType') == 'webui')
      login.SigninScreen.register();
    else
      login.GaiaSigninScreen.register();
    oobe.UserImageScreen.register();

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
        this.hidden = true;
        $('cancel-add-user-button').hidden = false;
        chrome.send('showAddUser');
      } else {
        $('bubble').showTextForElement($('add-user-button'),
            localStrings.getString('addUserOfflineMessage'));
      }
    });
    $('cancel-add-user-button').addEventListener('click', function(e) {
      this.hidden = true;
      $('add-user-button').hidden = false;
      Oobe.showScreen({id: 'account-picker'});
    });

    document.addEventListener('keydown', function(e) {
      Oobe.getInstance().oobeKeyDown(e);
    }, true);

    chrome.send('screenStateInitialize', []);
  };

  /**
   * Shows the given screen.
   * @param {string} screenId Id of the screen to show.
   */
  Oobe.showScreen = function(screenId) {
    Oobe.getInstance().showScreen(screenId);
  };

  /**
   * Enables/disables continue button.
   * @param {bool} enable Should the button be enabled?
   */
  Oobe.enableContinueButton = function(enable) {
    $('continue-button').disabled = !enable;
  };

  /**
   * Refreshes position of the network control (on connect screen).
   */
  Oobe.refreshNetworkControl = function() {
    var controlOffset = Oobe.getOffset($('network-control'));
    chrome.send('networkControlPosition',
                [controlOffset.left, controlOffset.top]);
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

    // Update the network control position.
    Oobe.refreshNetworkControl();
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

    // Don't show header bar and in-pace signin button for OOBE.
    $('login-header-bar').hidden = showOobe;
    $('signin-button-in-place').hidden = showOobe;
  };

  /**
   * Returns true if Oobe UI is shown.
   */
  Oobe.isOobeUI = function() {
    return !document.body.classList.contains('login-display');
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
