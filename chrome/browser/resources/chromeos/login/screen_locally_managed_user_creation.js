// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Locally managed user creation flow screen.
 */

cr.define('login', function() {
  /**
   * Creates a new screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var LocallyManagedUserCreationScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  LocallyManagedUserCreationScreen.register = function() {
    var screen = $('managed-user-creation-flow');
    LocallyManagedUserCreationScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  LocallyManagedUserCreationScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      //TODO(antrim) : add event listeners etc.
      this.showFinishedMessage();
    },

    /**
     * Screen controls.
     * @type {!Array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var finishButton = this.ownerDocument.createElement('button');
      finishButton.id = 'managed-user-creation-flow-finish-button';

      finishButton.textContent = loadTimeData.
          getString('managedUserCreationFlowFinishButtonTitle');
      finishButton.hidden = true;
      buttons.push(finishButton);

      var retryButton = this.ownerDocument.createElement('button');
      retryButton.id = 'managed-user-creation-flow-retry-button';

      retryButton.textContent = loadTimeData.
          getString('managedUserCreationFlowRetryButtonTitle');
      retryButton.hidden = true;
      buttons.push(retryButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'managed-user-creation-flow-cancel-button';

      cancelButton.textContent = loadTimeData.
          getString('managedUserCreationFlowCancelButtonTitle');
      cancelButton.hidden = true;
      buttons.push(cancelButton);

      var creationFlowScreen = this;
      finishButton.addEventListener('click', function(e) {
        creationFlowScreen.finishFlow_();
        e.stopPropagation();
      });
      retryButton.addEventListener('click', function(e) {
        creationFlowScreen.retryFlow_();
        e.stopPropagation();
      });
      cancelButton.addEventListener('click', function(e) {
        creationFlowScreen.abortFlow_();
        e.stopPropagation();
      });

      return buttons;
    },

    /**
     * Show final splash screen with success message.
     */
    showFinishedMessage: function() {
      this.setVisiblePage_('success');
      this.setVisibleButtons_(['finish']);
    },

    /**
     * Show error message.
     * @param {string} errorText Text to be displayed.
     * @param {boolean} recoverable Indicates if error was transiend and process
     *     can be retried.
     */
    showErrorMessage: function(errorText, recoverable) {
      $('managed-user-creation-flow-error-value').innerHTML = errorText;
      this.setVisiblePage_('error');
      this.setVisibleButtons_(recoverable ? ['retry', 'cancel'] : ['cancel']);
    },

    /**
     * Enables one particular subpage and hides the rest.
     * @param {string} visiblePage - name of subpage (one of 'progress',
     * 'error', 'success')
     * @private
     */
    setVisiblePage_: function(visiblePage) {
      var screenNames = ['progress', 'error', 'success'];
      for (i in screenNames) {
        var screenName = screenNames[i];
        var screen = $('managed-user-creation-flow-' + screenName);
        screen.hidden = (screenName != visiblePage);
      }
    },

    /**
     * Enables specific control buttons.
     * @param {List of strings} buttonsList - list of buttons to display (values
     * can be 'retry', 'finish', 'cancel')
     * @private
     */
    setVisibleButtons_: function(buttonsList) {
      var buttonNames = ['retry', 'finish', 'cancel'];
      for (i in buttonNames) {
        var buttonName = buttonNames[i];
        var button = $('managed-user-creation-flow-' + buttonName + '-button');
        if (button)
          button.hidden = buttonsList.indexOf(buttonName) < 0;
      }
    },

    finishFlow_: function() {
      chrome.send('finishLocalManagedUserCreation');
    },

    retryFlow_: function() {
      this.setVisiblePage_('progress');
      chrome.send('retryLocalManagedUserCreation');
    },

    abortFlow_: function() {
      chrome.send('abortLocalManagedUserCreation');
    },

    /**
     * Updates state of login header so that necessary buttons are displayed.
     **/
    onBeforeShow: function(data) {
      $('login-header-bar').signinUIState =
          SIGNIN_UI_STATE.MANAGED_USER_CREATION_FLOW;
    },

  };

  LocallyManagedUserCreationScreen.showFinishedMessage = function() {
    var screen = $('managed-user-creation-flow');
    screen.showFinishedMessage();
  };

  LocallyManagedUserCreationScreen.showErrorMessage = function(errorText,
                                                               recoverable) {
    var screen = $('managed-user-creation-flow');
    screen.showErrorMessage(errorText, recoverable);
  };

  return {
    LocallyManagedUserCreationScreen: LocallyManagedUserCreationScreen
  };
});

