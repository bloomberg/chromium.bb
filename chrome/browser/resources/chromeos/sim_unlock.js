// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  function SimUnlock() {
  }

  cr.addSingletonGetter(SimUnlock);

  SimUnlock.SIM_UNLOCK_LOADING           = -1;
  SimUnlock.SIM_ABSENT_NOT_LOCKED        =  0,
  SimUnlock.SIM_NOT_LOCKED_ASK_PIN       =  1;
  SimUnlock.SIM_LOCKED_PIN               =  2;
  SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT =  3;
  SimUnlock.SIM_LOCKED_PUK               =  4;
  SimUnlock.SIM_LOCKED_NO_PUK_TRIES_LEFT =  5;
  SimUnlock.SIM_DISABLED                 =  6;

  SimUnlock.ERROR_PIN = 'incorrectPin';
  SimUnlock.ERROR_PUK = 'incorrectPuk';
  SimUnlock.ERROR_OK = 'ok';

  SimUnlock.localStrings_ = new LocalStrings();

  SimUnlock.prototype = {
    initialized_: false,
    // True if when entering PIN we're changing PinRequired preference.
    changingPinRequiredPref_: false,
    pinRequiredNewValue_: false,
    state_: -1,

    changeState_: function(simInfo) {
      var newState = simInfo.state;
      var error = simInfo.error;
      var tries = simInfo.tries;
      this.hideAll_();
      switch(newState) {
        case SimUnlock.SIM_UNLOCK_LOADING:
          break;
        case SimUnlock.SIM_ABSENT_NOT_LOCKED:
          SimUnlock.close();
          break;
        case SimUnlock.SIM_LOCKED_PIN:
          var pinMessage;
          if (error == SimUnlock.ERROR_OK) {
            pinMessage = SimUnlock.localStrings_.getString('enterPinMessage');
            $('pin-error-msg').classList.remove('error');
          }
          if (error == SimUnlock.ERROR_PIN) {
            if (tries && tries >= 0) {
              pinMessage = SimUnlock.localStrings_.getStringF(
                  'enterPinTriesMessage', tries);
            } else {
              pinMessage = SimUnlock.localStrings_.getString('enterPinMessage');
            }
            $('pin-error-msg').classList.add('error');
          }
          $('pin-error-msg').textContent = pinMessage;
        case SimUnlock.SIM_NOT_LOCKED_ASK_PIN:
          $('pin-input').value = '';
          SimUnlock.enablePinDialog(true);
          $('locked-pin-overlay').hidden = false;
          break;
        case SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT:
          $('locked-pin-no-tries-overlay').hidden = false;
          break;
        case SimUnlock.SIM_LOCKED_PUK:
          $('puk-input').value = '';
          SimUnlock.enablePukDialog(true);
          if (tries && tries >= 0) {
            var pukMessage = SimUnlock.localStrings_.getStringF(
                'enterPukWarning', tries);
            $('puk-warning-msg').textContent = pukMessage;
          }
          $('locked-puk-overlay').hidden = false;
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

  SimUnlock.cancel = function() {
    chrome.send('cancel');
    SimUnlock.close();
  };

  SimUnlock.close = function() {
    window.close();
  };

  SimUnlock.initialize = function() {
    this.initialized_ = true;

    var pinReqPattern = /(^\?|&)pin-req=([^&#]*)/;
    var results = pinReqPattern.exec(window.location.search);
    if (results == null) {
      this.changingPinRequiredPref_ = false;
      this.pinRequiredNewValue_ = false;
    } else {
      this.changingPinRequiredPref_ = true;
      this.pinRequiredNewValue_ = /^true$/.test(results[2]);
    }

    $('enter-pin-confirm').addEventListener('click', function(event) {
      SimUnlock.enablePinDialog(false);
      chrome.send('enterPinCode', [$('pin-input').value]);
    });
    $('enter-pin-dismiss').addEventListener('click', function(event) {
      SimUnlock.cancel();
    });
    $('pin-no-tries-proceed').addEventListener('click', function(event) {
      chrome.send('proceedToPukInput');
    });
    $('pin-no-tries-dismiss').addEventListener('click', function(event) {
      SimUnlock.cancel();
    });
    $('enter-puk-confirm').addEventListener('click', function(event) {
      SimUnlock.enablePinDialog(false);
      chrome.send('enterPukCode', [$('puk-input').value]);
    });
    $('enter-puk-dismiss').addEventListener('click', function(event) {
      SimUnlock.cancel();
    });
    $('puk-no-tries-confirm').addEventListener('click', function(event) {
      SimUnlock.close();
    });
    $('sim-disabled-confirm').addEventListener('click', function(event) {
      SimUnlock.close();
    });
    chrome.send('simStatusInitialize', [this.changingPinRequiredPref_,
                                        this.pinRequiredNewValue_]);
  };

  SimUnlock.enablePinDialog = function(enabled) {
    $('pin-input').disabled = !enabled;
    $('enter-pin-confirm').disabled = !enabled;
    $('enter-pin-dismiss').disabled = !enabled;
  };

  SimUnlock.enablePukDialog = function(enabled) {
    $('puk-input').disabled = !enabled;
    $('enter-puk-confirm').disabled = !enabled;
    $('enter-puk-dismiss').disabled = !enabled;
  };

  SimUnlock.simStateChanged = function(simInfo) {
    SimUnlock.getInstance().updateSimStatus_(simInfo);
  };

  // Export
  return {
    SimUnlock: SimUnlock
  };

});
