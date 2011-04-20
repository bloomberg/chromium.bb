// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  function SimUnlock() {
  }

  cr.addSingletonGetter(SimUnlock);

  // State of the dialog.
  SimUnlock.SIM_UNLOCK_LOADING           = -1;
  SimUnlock.SIM_ABSENT_NOT_LOCKED        =  0,
  SimUnlock.SIM_NOT_LOCKED_ASK_PIN       =  1;
  SimUnlock.SIM_NOT_LOCKED_CHANGE_PIN    =  2;
  SimUnlock.SIM_LOCKED_PIN               =  3;
  SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT =  4;
  SimUnlock.SIM_LOCKED_PUK               =  5;
  SimUnlock.SIM_LOCKED_NO_PUK_TRIES_LEFT =  6;
  SimUnlock.SIM_DISABLED                 =  7;

  // Mode of the dialog.
  SimUnlock.SIM_DIALOG_UNLOCK       = 0;
  SimUnlock.SIM_DIALOG_CHANGE_PIN   = 1;
  SimUnlock.SIM_DIALOG_SET_LOCK_ON  = 2;
  SimUnlock.SIM_DIALOG_SET_LOCK_OFF = 3;

  // Error codes.
  SimUnlock.ERROR_PIN = 'incorrectPin';
  SimUnlock.ERROR_PUK = 'incorrectPuk';
  SimUnlock.ERROR_OK = 'ok';

  SimUnlock.localStrings_ = new LocalStrings();

  SimUnlock.prototype = {
    initialized_: false,
    mode_: SimUnlock.SIM_DIALOG_UNLOCK,
    pukValue_: '',
    state_: SimUnlock.SIM_UNLOCK_LOADING,

    changeState_: function(simInfo) {
      var newState = simInfo.state;
      var error = simInfo.error;
      var tries = simInfo.tries;
      var pinMessage;
      this.hideAll_();
      switch(newState) {
        case SimUnlock.SIM_UNLOCK_LOADING:
          break;
        case SimUnlock.SIM_ABSENT_NOT_LOCKED:
          SimUnlock.close();
          break;
        case SimUnlock.SIM_LOCKED_PIN:
          if (error == SimUnlock.ERROR_OK) {
            pinMessage = SimUnlock.localStrings_.getStringF(
                'enterPinTriesMessage', tries);
            $('pin-error-msg').classList.remove('error');
          } else if (error == SimUnlock.ERROR_PIN) {
              pinMessage = SimUnlock.localStrings_.getStringF(
                  'incorrectPinTriesMessage', tries);
            $('pin-error-msg').classList.add('error');
          }
          $('pin-error-msg').textContent = pinMessage;
          $('pin-input').value = '';
          SimUnlock.enablePinDialog(true);
          $('locked-pin-overlay').hidden = false;
          $('pin-input').focus();
          break;
        case SimUnlock.SIM_NOT_LOCKED_ASK_PIN:
          if (error == SimUnlock.ERROR_OK) {
            pinMessage = SimUnlock.localStrings_.getString('enterPinMessage');
            $('pin-error-msg').classList.remove('error');
          } else if (error == SimUnlock.ERROR_PIN) {
              pinMessage = SimUnlock.localStrings_.getStringF(
                  'incorrectPinTriesMessage', tries);
            $('pin-error-msg').classList.add('error');
          }
          $('pin-error-msg').textContent = pinMessage;
          $('pin-input').value = '';
          SimUnlock.enablePinDialog(true);
          $('locked-pin-overlay').hidden = false;
          $('pin-input').focus();
          break;
        case SimUnlock.SIM_NOT_LOCKED_CHANGE_PIN:
          SimUnlock.prepareChoosePinDialog(true);
          if (error == SimUnlock.ERROR_OK) {
            pinMessage = SimUnlock.localStrings_.getString('changePinMessage');
            $('choose-pin-msg').classList.remove('error');
          } else if (error == SimUnlock.ERROR_PIN) {
              pinMessage = SimUnlock.localStrings_.getStringF(
                  'incorrectPinTriesMessage', tries);
            $('choose-pin-msg').classList.add('error');
          }
          $('choose-pin-msg').textContent = pinMessage;
          $('old-pin-input').value = '';
          $('new-pin-input').value = '';
          $('retype-new-pin-input').value = '';
          $('choose-pin-overlay').hidden = false;
          $('old-pin-input').focus();
          SimUnlock.enableChoosePinDialog(true);
          break;
        case SimUnlock.SIM_LOCKED_NO_PIN_TRIES_LEFT:
          $('locked-pin-no-tries-overlay').hidden = false;
          break;
        case SimUnlock.SIM_LOCKED_PUK:
          $('puk-input').value = '';
          if (tries && tries >= 0) {
            var pukMessage = SimUnlock.localStrings_.getStringF(
                'enterPukWarning', tries);
            $('puk-warning-msg').textContent = pukMessage;
          }
          $('locked-puk-overlay').hidden = false;
          $('puk-input').focus();
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
      $('choose-pin-overlay').hidden = true;
      $('locked-puk-no-tries-overlay').hidden = true;
      $('sim-disabled-overlay').hidden = true;
    },

    newPinEntered_: function(newPin, newPin2) {
      var changePinMode = this.state_ == SimUnlock.SIM_NOT_LOCKED_CHANGE_PIN;
      if (newPin != newPin2) {
        $('choose-pin-error').hidden = false;
        $('old-pin-input').value = '';
        $('new-pin-input').value = '';
        $('retype-new-pin-input').value = '';
        if (changePinMode)
          $('old-pin-input').focus();
        else
          $('new-pin-input').focus();
      } else {
        $('choose-pin-error').hidden = true;
        SimUnlock.enableChoosePinDialog(false);
        if (changePinMode) {
          var oldPin = $('old-pin-input').value;
          chrome.send('changePinCode', [oldPin, newPin]);
        } else {
          chrome.send('enterPukCode', [this.pukValue_, newPin]);
          this.pukValue_ = '';
        }
      }
    },

    pukEntered_: function(pukValue) {
      this.pukValue_ = pukValue;
      this.hideAll_();
      SimUnlock.prepareChoosePinDialog(false);
      SimUnlock.enableChoosePinDialog(true);
      $('new-pin-input').value = '';
      $('retype-new-pin-input').value = '';
      $('choose-pin-overlay').hidden = false;
      $('new-pin-input').focus();
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

    var modePattern = /(^\?|&)mode=([^&#]*)/;
    var results = modePattern.exec(window.location.search);
    if (results == null) {
      this.mode_ = SimUnlock.SIM_DIALOG_UNLOCK;
    } else {
      var mode = results[2];
      if (mode == 'change-pin')
        this.mode_ = SimUnlock.SIM_DIALOG_CHANGE_PIN;
      else if (mode == 'set-lock-on')
        this.mode_ = SimUnlock.SIM_DIALOG_SET_LOCK_ON;
      else if (mode == 'set-lock-off')
        this.mode_ = SimUnlock.SIM_DIALOG_SET_LOCK_OFF;
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
      SimUnlock.pukEntered($('puk-input').value);
    });
    $('enter-puk-dismiss').addEventListener('click', function(event) {
      SimUnlock.cancel();
    });
    $('choose-pin-confirm').addEventListener('click', function(event) {
      SimUnlock.newPinEntered($('new-pin-input').value,
                              $('retype-new-pin-input').value);
    });
    $('choose-pin-dismiss').addEventListener('click', function(event) {
      SimUnlock.cancel();
    });
    $('puk-no-tries-confirm').addEventListener('click', function(event) {
      SimUnlock.close();
    });
    $('sim-disabled-confirm').addEventListener('click', function(event) {
      SimUnlock.close();
    });
    chrome.send('simStatusInitialize', [this.mode_]);
  };

  SimUnlock.enablePinDialog = function(enabled) {
    $('pin-input').disabled = !enabled;
    $('enter-pin-confirm').disabled = !enabled;
    $('enter-pin-dismiss').disabled = !enabled;
  };

  SimUnlock.enableChoosePinDialog = function(enabled) {
    $('old-pin-input').disabled = !enabled;
    $('new-pin-input').disabled = !enabled;
    $('retype-new-pin-input').disabled = !enabled;
    $('choose-pin-confirm').disabled = !enabled;
    $('choose-pin-dismiss').disabled = !enabled;
  };

  SimUnlock.prepareChoosePinDialog = function(changePin) {
    // Our dialog has different height than choose-pin step of the
    // unlock process which we're reusing.
    if (changePin) {
      $('choose-pin-content-area').classList.remove('choose-pin-content-area');
      $('choose-pin-content-area').classList.add('change-pin-content-area');
      var title = SimUnlock.localStrings_.getString('changePinTitle');
      $('choose-pin-title').textContent = title;
    } else {
      $('choose-pin-content-area').classList.remove('change-pin-content-area');
      $('choose-pin-content-area').classList.add('choose-pin-content-area');
      var pinMessage = SimUnlock.localStrings_.getString('choosePinMessage');
      $('choose-pin-msg').classList.remove('error');
      $('choose-pin-msg').textContent = pinMessage;
      var title = SimUnlock.localStrings_.getString('choosePinTitle');
      $('choose-pin-title').textContent = title;
    }
    $('old-pin-label').hidden = !changePin;
    $('old-pin-input-area').hidden = !changePin;
  };

  SimUnlock.newPinEntered = function(newPin, newPin2) {
    SimUnlock.getInstance().newPinEntered_(newPin, newPin2);
  };

  SimUnlock.pukEntered = function(pukValue) {
    SimUnlock.getInstance().pukEntered_(pukValue);
  };

  SimUnlock.simStateChanged = function(simInfo) {
    SimUnlock.getInstance().updateSimStatus_(simInfo);
  };

  // Export
  return {
    SimUnlock: SimUnlock
  };

});
