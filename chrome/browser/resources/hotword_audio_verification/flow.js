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
     * Prefix of the element ids for the page that is currently training.
     * @private {string}
     */
    this.trainingPagePrefix_ = '';
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

    if (chrome.hotwordPrivate.onHotwordTriggered) {
      chrome.hotwordPrivate.onHotwordTriggered.addListener(
          this.handleHotwordTrigger_.bind(this));
    }
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
          removeListener(this.handleHotwordTrigger_);
    }
    if (chrome.hotwordPrivate.stopTraining)
      chrome.hotwordPrivate.stopTraining();
  };

  /**
   * Handles the speaker model finalized event.
   */
  Flow.prototype.onSpeakerModelFinalized = function() {
    this.stopTraining();

    if (chrome.hotwordPrivate.setAudioLoggingEnabled)
      chrome.hotwordPrivate.setAudioLoggingEnabled(true, function() {});

    if (chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled) {
      chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled(true,
          this.advanceStep.bind(this));
    }
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
   * Handles a hotword trigger event and updates the training UI.
   * @private
   */
  Flow.prototype.handleHotwordTrigger_ = function() {
    var curStep =
        $(this.trainingPagePrefix_ + '-training').querySelector('.listening');
    // TODO(kcarattini): Localize this string.
    curStep.querySelector('.text').textContent = 'Recorded';
    curStep.classList.remove('listening');
    curStep.classList.add('recorded');

    var steps =
        $(this.trainingPagePrefix_ + '-training').querySelectorAll('.train');
    var index = Array.prototype.indexOf.call(steps, curStep);
    if (steps[index + 1]) {
      steps[index + 1].classList.remove('not-started');
      steps[index + 1].classList.add('listening');
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
