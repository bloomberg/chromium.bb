// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @class FunctionSequence to invoke steps in sequence
 *
 * @param steps             array of functions to invoke in sequence
 * @param callback          callback to invoke on success
 * @param failureCallback   callback to invoke on failure
 */
function FunctionSequence(steps, callback, failureCallback, logger) {
  // Private variables hidden in closure
  var _currentStepIdx = -1;
  var _self = this;
  var _failed = false;

  this.onError = function(err) {
    logger.vlog('Failed step: ' + steps[_currentStepIdx].name + ': ' + err);
    if (!_failed) {
      _failed = true;
      failureCallback.apply(this, err, steps[_currentStepIdx].name);
    }
  };

  this.finish = function() {
    if (!_failed) {
      _currentStepIdx = steps.length;
      callback();
    }
  };

  this.nextStep = function(var_args) {
    logger.vlog('Starting new step');

    if (_failed) {
      return;
    }

    _currentStepIdx++;

    if (_currentStepIdx >= steps.length) {
      callback.apply(_self, arguments);
      return;
    }

    var currentStep = steps[_currentStepIdx];

    logger.vlog('Current step type is' + (typeof currentStep));

    if (typeof currentStep == 'function') {
      logger.vlog('nextstep : ' + currentStep.name);
      currentStep.apply(_self, arguments);
    } else {
      logger.vlog('nextsep forking ' + currentStep.length + ' chains');
      var remaining = currentStep.length;

      function resume() {
        if (--remaining == 0) {
          logger.vlog('barrier passed');
          _self.nextStep();
        } else {
          logger.vlog('waiting for [' + remaining + '] at barrier');
        }
      }

      for (var i = 0; i < currentStep.length; i++) {
        var sequence = new FunctionSequence(currentStep[i], resume,
            _self.onError, logger);
        sequence.start.apply(sequence, arguments);
      }
    }
  };

  this.start = this.nextStep;
}
