// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device reset screen implementation.
 */

login.createScreen('ResetScreen', 'reset', function() {
  return {

    /* Possible UI states of the reset screen. */
    RESET_SCREEN_UI_STATE: {
      REVERT_PROMISE: 'ui-state-revert-promise',
      RESTART_REQUIRED: 'ui-state-restart-required',
      POWERWASH_PROPOSAL: 'ui-state-powerwash-proposal',
      POWERWASH_CONFIRM: 'ui-state-powerwash-confirm',
      ROLLBACK_CONFIRM: 'ui-state-rollback-confirm'
    },

    EXTERNAL_API: [
      'hideRollbackOption',
      'showRollbackOption',
      'updateViewOnRollbackCall'
    ],

    /** @override */
    decorate: function() {
      $('powerwash-help-link').addEventListener('click', function(event) {
        chrome.send('resetOnLearnMore');
      });
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('resetScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var restartButton = this.ownerDocument.createElement('button');
      restartButton.id = 'reset-restart-button';
      restartButton.textContent = loadTimeData.getString('resetButtonRestart');
      restartButton.addEventListener('click', function(e) {
        chrome.send('restartOnReset');
        e.stopPropagation();
      });
      buttons.push(restartButton);

      // Button that initiates actual powerwash or powerwash with rollback.
      var resetButton = this.ownerDocument.createElement('button');
      resetButton.id = 'reset-button';
      resetButton.textContent = loadTimeData.getString('resetButtonReset');
      resetButton.addEventListener('click', function(e) {
        chrome.send('powerwashOnReset', [$('reset').rollbackChecked]);
        e.stopPropagation();
      });
      buttons.push(resetButton);

      // Button that leads to confirmation dialog.
      var toConfirmButton = this.ownerDocument.createElement('button');
      toConfirmButton.id = 'reset-toconfirm-button';
      toConfirmButton.textContent =
          loadTimeData.getString('resetButtonPowerwash');
      toConfirmButton.addEventListener('click', function(e) {
        // change view to confirmational
        var resetScreen = $('reset');
        resetScreen.isConfirmational = true;
        if (resetScreen.rollbackChecked && resetScreen.rollbackAvailable) {
          resetScreen.setDialogView_(
              resetScreen.RESET_SCREEN_UI_STATE.ROLLBACK_CONFIRM);
        } else {
          resetScreen.setDialogView_(
              resetScreen.RESET_SCREEN_UI_STATE.POWERWASH_CONFIRM);
        }
        chrome.send('showConfirmationOnReset');
        e.stopPropagation();
      });
      buttons.push(toConfirmButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'reset-cancel-button';
      cancelButton.textContent = loadTimeData.getString('cancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('cancelOnReset');
        e.stopPropagation();
      });
      buttons.push(cancelButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      // choose
      if (this.needRestart)
        return $('reset-restart-button');
      if (this.isConfirmational)
        if (this.rollbackChecked)
          return $('reset-button');
      else
        return $('reset-toconfirm-button');
      return $('reset-button');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('cancelOnReset');
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      if (data === undefined)
        return;

      this.rollbackChecked = false;
      this.rollbackAvailable = false;
      this.isConfirmational = false;

      if ('rollbackAvailable' in data)
        this.rollbackAvailable = data['rollbackAvailable'];

      if ('restartRequired' in data && data['restartRequired']) {
        this.restartRequired = true;
        this.setDialogView_(this.RESET_SCREEN_UI_STATE.RESTART_REQUIRED);
      } else {
        this.restartRequired = false;
        this.setDialogView_(this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL);
      }
    },

    /**
      * Sets css style for corresponding state of the screen.
      * @param {string} state.
      * @private
      */
    setDialogView_: function(state) {
      this.classList.remove('revert-promise-view');
      this.classList.remove('restart-required-view');
      this.classList.remove('powerwash-proposal-view');
      this.classList.remove('powerwash-confirm-view');
      this.classList.remove('rollback-confirm-view');
      if (state == this.RESET_SCREEN_UI_STATE.REVERT_PROMISE)
        this.classList.add('revert-promise-view');
      else if (state == this.RESET_SCREEN_UI_STATE.RESTART_REQUIRED)
        this.classList.add('restart-required-view');
      else if (state == this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL)
        this.classList.add('powerwash-proposal-view');
      else if (state == this.RESET_SCREEN_UI_STATE.POWERWASH_CONFIRM)
        this.classList.add('powerwash-confirm-view');
      else if (state == this.RESET_SCREEN_UI_STATE.ROLLBACK_CONFIRM)
        this.classList.add('rollback-confirm-view');
      else  // error
        console.error('State ' + state + ' is not supported by setDialogView.');
    },

    updateViewOnRollbackCall: function() {
      this.setDialogView_(this.RESET_SCREEN_UI_STATE.REVERT_PROMISE);
      announceAccessibleMessage(
          loadTimeData.getString('resetRevertSpinnerMessage'));
    },

    showRollbackOption: function() {
      $('reset-toconfirm-button').textContent = loadTimeData.getString(
          'resetButtonPowerwashAndRollback');
      this.rollbackChecked = true;
    },

    hideRollbackOption: function() {
      $('reset-toconfirm-button').textContent = loadTimeData.getString(
          'resetButtonPowerwash');
      this.rollbackChecked = false;
    }
  };
});
