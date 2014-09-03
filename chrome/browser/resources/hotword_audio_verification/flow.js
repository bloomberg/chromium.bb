// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

  // Correspond to steps in the hotword opt-in flow.
  /** @const */ var HOTWORD_AUDIO_HISTORY = 'hotword-audio-history-container';
  /** @const */ var HOTWORD_ONLY_START = 'hotword-only-container';
  /** @const */ var AUDIO_HISTORY_START = 'audio-history-container';
  /** @const */ var SPEECH_TRAINING = 'speech-training-container';
  /** @const */ var FINISHED = 'finished-container';

  /** @const */ var FLOWS = {
    HOTWORD_AND_AUDIO_HISTORY: [
        HOTWORD_AUDIO_HISTORY, SPEECH_TRAINING, FINISHED],
    HOTWORD_ONLY: [HOTWORD_ONLY_START, SPEECH_TRAINING, FINISHED],
    AUDIO_HISTORY_ONLY: [AUDIO_HISTORY_START]
  };

  /**
   * Class to control the page flow of the always-on hotword and
   * Audio History opt-in process.
   * @constructor
   */
  function Flow() {
    this.currentStepIndex_ = -1;
    this.currentFlow_ = [];
  }

  /**
   * Gets the appropriate flow and displays its first page.
   */
  Flow.prototype.startFlow = function() {
    this.currentFlow_ = getFlowForSetting_.apply(this);
    this.advanceStep();
  };

  /**
   * Advances the current step.
   */
  Flow.prototype.advanceStep = function() {
    this.currentStepIndex_++;
    if (this.currentStepIndex_ < this.currentFlow_.length)
      showStep_.apply(this);
  };

  // ---- private methods:

  /**
   * Gets the appropriate flow for the current configuration of settings.
   * @private
   */
  getFlowForSetting_ = function() {
    // TODO(kcarattini): This should eventually return the correct flow for
    // the current settings state.
    return FLOWS.HOTWORD_AND_AUDIO_HISTORY;
  };

  /**
   * Displays the current step. If the current step is not the first step,
   * also hides the previous step.
   * @private
   */
  showStep_ = function() {
    var currentStep = this.currentFlow_[this.currentStepIndex_];
    var previousStep = null;
    if (this.currentStepIndex_ > 0)
      previousStep = this.currentFlow_[this.currentStepIndex_ - 1];

    if (previousStep)
      document.getElementById(previousStep).hidden = true;

    document.getElementById(currentStep).hidden = false;
  };

  window.Flow = Flow;
})();
