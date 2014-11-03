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
   * Class to control the page flow of the always-on hotword and
   * Audio History opt-in process.
   * @constructor
   */
  function Flow() {
    this.currentStepIndex_ = -1;
    this.currentFlow_ = [];
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

  // ---- private methods:

  /**
   * Gets and starts the appropriate flow for the launch mode.
   * @private
   */
  Flow.prototype.startFlowForMode_ = function(state) {
    assert(state.launchMode >= 0 && state.launchMode < FLOWS.length,
           'Invalid Launch Mode.');
    this.currentFlow_ = FLOWS[state.launchMode];
    this.advanceStep();
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
