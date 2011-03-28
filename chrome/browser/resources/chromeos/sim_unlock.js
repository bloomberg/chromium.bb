// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  function SimUnlock() {
  }

  cr.addSingletonGetter(SimUnlock);

  SimUnlock.SIM_UNLOCK_LOADING           = -1;
  SimUnlock.SIM_ABSENT_NOT_LOCKED        =  0,
  SimUnlock.SIM_LOCKED_PIN               =  1;
  SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT =  2;
  SimUnlock.SIM_LOCKED_PUK               =  3;
  SimUnlock.SIM_LOCKED_NO_PUK_TRIES_LEFT =  4;
  SimUnlock.SIM_DISABLED                 =  5;

  SimUnlock.ERROR_PIN = 'incorrectPin';
  SimUnlock.ERROR_PUK = 'incorrectPuk';
  SimUnlock.ERROR_OK = 'ok';

  SimUnlock.localStrings_ = new LocalStrings();

  SimUnlock.prototype = {
    initialized_: false,
    state_: -1,

    changeState_: function(simInfo) {
      var newState = simInfo.state;
      var error = simInfo.error;
      this.hideAll_();
      switch(newState) {
        case SimUnlock.SIM_UNLOCK_LOADING:
          break;
        case SimUnlock.SIM_ABSENT_NOT_LOCKED:
          // TODO(nkostylev): Update status info, probably close dialog.
          break;
        case SimUnlock.SIM_LOCKED_PIN:
          $('pin-input').value = '';
          SimUnlock.enablePinDialog(true);
          $('locked-pin-overlay').hidden = false;
          var pinMessage;
          if (error == SimUnlock.ERROR_OK) {
            pinMessage = SimUnlock.localStrings_.getString('enterPinMessage');
            $('pin-error-msg').classList.remove('error');
          }
          if (error == SimUnlock.ERROR_PIN) {
            pinMessage =
                SimUnlock.localStrings_.getString('incorrectPinMessage');
            $('pin-error-msg').classList.add('error');
          }
          $('pin-error-msg').textContent = pinMessage;
          // TODO(nkostylev): Show pin tries left info.
          break;
        case SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT:
          $('locked-pin-no-tries-overlay').hidden = false;
          break;
        case SimUnlock.SIM_LOCKED_PUK:
          $('locked-puk-overlay').hidden = false;
          // TODO(nkostylev): Show PUK warning.
          // TODO(nkostylev): Show PUK tries left info.
          break;
        case SimUnlock.SIM_LOCKED_NO_PUK_TRIES_LEFT:
          $('locked-puk-no-tries-overlay').hidden = false;
          break;
        case SimUnlock.SimUnlock.SIM_DISABLED:
          $('sim-disabled-overlay').hidden = false;
          break;
      }
      this.state_ = newState;
    },

    hideAll_: function() {
      $('locked-pin-overlay').hidden = true;
      $('locked-pin-no-tries-overlay').hidden = true;
      $('locked-puk-overlay').hidden = true;
      $('locked-puk-no-tries-overlay').hidden = true;
      $('sim-disabled-overlay').hidden = true;
    },

    updateSimStatus_: function(simInfo) {
      this.changeState_(simInfo);
    },
  };

  SimUnlock.close = function() {
    // TODO(nkostylev): Close dialog.
  };

  SimUnlock.initialize = function() {
    this.initialized_ = true;
    $('enter-pin-confirm').addEventListener('click', function(event) {
      SimUnlock.enablePinDialog(false);
      chrome.send('enterPinCode', [$('pin-input').value]);
    });
    $('enter-pin-dismiss').addEventListener('click', function(event) {
      SimUnlock.close();
    });
    chrome.send('simStatusInitialize');
  };

  SimUnlock.enablePinDialog = function(enabled) {
    $('pin-input').disabled = !enabled;
    $('enter-pin-confirm').disabled = !enabled;
    $('enter-pin-dismiss').disabled = !enabled;
  };

  SimUnlock.simStateChanged = function(simInfo) {
    SimUnlock.getInstance().updateSimStatus_(simInfo);
  };

  // Export
  return {
    SimUnlock: SimUnlock
  };

});
