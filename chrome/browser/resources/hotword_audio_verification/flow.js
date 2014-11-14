// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

  // Correspond to steps in the hotword opt-in flow.
  /** @const */ var HOTWORD_AUDIO_HISTORY = 'hotword-audio-history-container';
  /** @const */ var HOTWORD_ONLY_START = 'hotword-only-container';
  /** @const */ var SPEECH_TRAINING = 'speech-training-container';
  /** @const */ var FINISHED = 'finished-container';

  /**
   * These flows correspond to the three LaunchModes as defined in
   * chrome/browser/search/hotword_service.h and should be kept in sync
   * with them.
   * @const
   */
  var FLOWS = [
    [HOTWORD_ONLY_START, FINISHED],
    [HOTWORD_AUDIO_HISTORY, SPEECH_TRAINING, FINISHED],
    [SPEECH_TRAINING, FINISHED]
  ];

  /**
   * The launch mode. This enum needs to be kept in sync with that of
   * the same name in hotword_service.h.
   * @enum {number}
   */
  var LaunchMode = {
    HOTWORD_ONLY: 0,
    HOTWORD_AND_AUDIO_HISTORY: 1,
    RETRAIN: 2
  };

  /**
   * The training state.
   * @enum {string}
   */
  var TrainingState = {
    RESET: 'reset',
    TIMEOUT: 'timeout',
  };

  /**
   * Class to control the page flow of the always-on hotword and
   * Audio History opt-in process.
   * @constructor
   */
  function Flow() {
    this.currentStepIndex_ = -1;
    this.currentFlow_ = [];

    /**
     * Whether this flow is currently in the process of training a voice model.
     * @private {LaunchMode}
     */
    this.launchMode_ = LaunchMode.HOTWORD_AND_AUDIO_HISTORY;

    /**
     * Whether this flow is currently in the process of training a voice model.
     * @private {boolean}
     */
    this.training_ = false;

    /**
     * The current training state.
     * @private {?TrainingState}
     */
    this.trainingState_ = null;

    /**
     * Whether an expected hotword trigger has been received, indexed by
     * training step.
     * @private {boolean[]}
     */
    this.hotwordTriggerReceived_ = [];

    /**
     * Prefix of the element ids for the page that is currently training.
     * @private {string}
     */
    this.trainingPagePrefix_ = '';

    /**
     * Chrome event listener. Saved so that it can be de-registered
     * when training stops.
     * @private {Function}
     */
    this.hotwordTriggeredListener_ = this.handleHotwordTrigger_.bind(this);
  }

  /**
   * Advances the current step.
   */
  Flow.prototype.advanceStep = function() {
    this.currentStepIndex_++;
    if (this.currentStepIndex_ < this.currentFlow_.length)
      this.showStep_.apply(this);
  };

  /**
   * Gets the appropriate flow and displays its first page.
   */
  Flow.prototype.startFlow = function() {
    if (chrome.hotwordPrivate && chrome.hotwordPrivate.getLaunchState)
      chrome.hotwordPrivate.getLaunchState(this.startFlowForMode_.bind(this));
  };

  /**
   * Starts the training process.
   */
  Flow.prototype.startTraining = function() {
    // Don't start a training session if one already exists.
    if (this.training_)
      return;

    this.training_ = true;
    if (this.launchMode_ == LaunchMode.HOTWORD_ONLY) {
      this.trainingPagePrefix_ = 'hotword-only';
    } else if (this.launchMode_ == LaunchMode.HOTWORD_AND_AUDIO_HISTORY ||
               this.launchMode_ == LaunchMode.RETRAIN) {
      this.trainingPagePrefix_ = 'speech-training';
    }

    if (chrome.hotwordPrivate.onHotwordTriggered &&
        !chrome.hotwordPrivate.onHotwordTriggered.hasListener(
            this.hotwordTriggeredListener_)) {
      chrome.hotwordPrivate.onHotwordTriggered.addListener(
          this.hotwordTriggeredListener_);
    }

    this.waitForHotwordTrigger_(0);
    if (chrome.hotwordPrivate.startTraining)
      chrome.hotwordPrivate.startTraining();
  };

  /**
   * Stops the training process.
   */
  Flow.prototype.stopTraining = function() {
    if (!this.training_)
      return;

    this.training_ = false;
    if (chrome.hotwordPrivate.onHotwordTriggered) {
      chrome.hotwordPrivate.onHotwordTriggered.
          removeListener(this.hotwordTriggeredListener_);
    }
    if (chrome.hotwordPrivate.stopTraining)
      chrome.hotwordPrivate.stopTraining();
  };

  /**
   * Handles the speaker model finalized event.
   */
  Flow.prototype.onSpeakerModelFinalized = function() {
    this.stopTraining();

    if (chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled) {
      chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled(true,
          this.advanceStep.bind(this));
    }
  };

  /**
   * Handles a user clicking on the retry button.
   */
  Flow.prototype.handleRetry = function() {
    if (this.trainingState_ != TrainingState.TIMEOUT)
      return;

    this.startTraining();
    this.updateTrainingState_(TrainingState.RESET);
  };

  // ---- private methods:

  /**
   * Completes the training process.
   * @private
   */
  Flow.prototype.finalizeSpeakerModel_ = function() {
    if (!this.training_)
      return;

    if (chrome.hotwordPrivate.finalizeSpeakerModel)
      chrome.hotwordPrivate.finalizeSpeakerModel();

    // TODO(kcarattini): Implement a notification that speaker model has been
    // finalized instead of setting a timeout.
    setTimeout(this.onSpeakerModelFinalized.bind(this), 2000);
  };

  /**
   * Returns the current training step.
   * @param {string} curStepClassName The name of the class of the current
   *     training step.
   * @return {Object} The current training step, its index, and an array of
   *     all training steps. Any of these can be undefined.
   * @private
   */
  Flow.prototype.getCurrentTrainingStep_ = function(curStepClassName) {
    var steps =
        $(this.trainingPagePrefix_ + '-training').querySelectorAll('.train');
    var curStep =
        $(this.trainingPagePrefix_ + '-training').querySelector('.listening');
    return {current: curStep,
            index: Array.prototype.indexOf.call(steps, curStep),
            steps: steps};
  };

  /**
   * Updates the training state.
   * @param {TrainingState} state The training state.
   * @private
   */
  Flow.prototype.updateTrainingState_ = function(state) {
    this.trainingState_ = state;
    this.updateErrorUI_();
  };

  /**
   * Waits two minutes and then checks for a training error.
   * @param {number} index The index of the training step.
   * @private
   */
  Flow.prototype.waitForHotwordTrigger_ = function(index) {
    if (!this.training_)
      return;

    this.hotwordTriggerReceived_[index] = false;
    setTimeout(this.handleTrainingTimeout_.bind(this, index), 120000);
  };

  /**
   * Checks for and handles a training error.
   * @param {number} index The index of the training step.
   * @private
   */
  Flow.prototype.handleTrainingTimeout_ = function(index) {
    if (!this.training_)
      return;

    if (this.hotwordTriggerReceived_[index])
      return;

    this.updateTrainingState_(TrainingState.TIMEOUT);
    this.stopTraining();
  };

  /**
   * Updates the training error UI.
   * @private
   */
  Flow.prototype.updateErrorUI_ = function() {
    if (!this.training_)
      return;

    var trainingSteps = this.getCurrentTrainingStep_('listening');
    var steps = trainingSteps.steps;

    if (this.trainingState_ == TrainingState.RESET) {
      // We reset the training to begin at the first step.
      // The first step is reset to 'listening', while the rest
      // are reset to 'not-started'.

      // TODO(kcarattini): Localize strings.
      var prompt = 'Say "Ok Google"';
      for (var i = 0; i < steps.length; ++i) {
        steps[i].classList.remove('recorded');
        if (i == 0) {
          steps[i].classList.remove('not-started');
          steps[i].classList.add('listening');
        } else {
          steps[i].classList.add('not-started');
          if (i == steps.length - 1)
            prompt += ' one last time';
          else
            prompt += ' again';
        }
        steps[i].querySelector('.text').textContent = prompt;
      }
    } else if (this.trainingState_ == TrainingState.TIMEOUT) {
      var curStep = trainingSteps.current;
      if (curStep) {
        curStep.classList.remove('listening');
        curStep.classList.add('not-started');
      }
    }
    $(this.trainingPagePrefix_ + '-timeout').hidden =
        !(this.trainingState_ == TrainingState.TIMEOUT);
  };

  /**
   * Handles a hotword trigger event and updates the training UI.
   * @private
   */
  Flow.prototype.handleHotwordTrigger_ = function() {
    var trainingSteps = this.getCurrentTrainingStep_('listening');

    if (!trainingSteps.current)
      return;

    var index = trainingSteps.index;
    this.hotwordTriggerReceived_[index] = true;

    // TODO(kcarattini): Localize this string.
    trainingSteps.current.querySelector('.text').textContent = 'Recorded';
    trainingSteps.current.classList.remove('listening');
    trainingSteps.current.classList.add('recorded');

    if (trainingSteps.steps[index + 1]) {
      trainingSteps.steps[index + 1].classList.remove('not-started');
      trainingSteps.steps[index + 1].classList.add('listening');
      this.waitForHotwordTrigger_(index + 1);
      return;
    }

    // Only the last step makes it here.
    var buttonElem = $(this.trainingPagePrefix_ + '-cancel-button');
    // TODO(kcarattini): Localize this string.
    buttonElem.textContent = 'Please wait ...';
    buttonElem.classList.add('grayed-out');
    buttonElem.classList.remove('finish-button');

    this.finalizeSpeakerModel_();
  };

  /**
   * Gets and starts the appropriate flow for the launch mode.
   * @param {chrome.hotwordPrivate.LaunchState} state Launch state of the
   *     Hotword Audio Verification App.
   * @private
   */
  Flow.prototype.startFlowForMode_ = function(state) {
    this.launchMode_ = state.launchMode;
    assert(state.launchMode >= 0 && state.launchMode < FLOWS.length,
           'Invalid Launch Mode.');
    this.currentFlow_ = FLOWS[state.launchMode];
    this.advanceStep();
    // If the flow begins with a a training step, then start the training flow.
    if (state.launchMode == LaunchMode.HOTWORD_ONLY ||
        state.launchMode == LaunchMode.RETRAIN) {
      this.startTraining();
    }
  };

  /**
   * Displays the current step. If the current step is not the first step,
   * also hides the previous step.
   * @private
   */
  Flow.prototype.showStep_ = function() {
    var currentStep = this.currentFlow_[this.currentStepIndex_];
    document.getElementById(currentStep).hidden = false;

    var previousStep = null;
    if (this.currentStepIndex_ > 0)
      previousStep = this.currentFlow_[this.currentStepIndex_ - 1];

    if (previousStep)
      document.getElementById(previousStep).hidden = true;

    chrome.app.window.current().show();
  };

  window.Flow = Flow;
})();
