// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to manage speaker training. Starts a hotwording session
   * if training is on, and automatically restarts the detector when a
   * a hotword is triggered.
   * @param {!hotword.StateManager} stateManager
   * @constructor
   * @extends {hotword.BaseSessionManager}
   */
  function TrainingManager(stateManager) {
    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     * @private
     */
    this.finalizedSpeakerModelListener_ =
        this.handleFinalizeSpeakerModel_.bind(this);

    hotword.BaseSessionManager.call(this,
                                    stateManager,
                                    hotword.constants.SessionSource.TRAINING);
  }

  TrainingManager.prototype = {
    __proto__: hotword.BaseSessionManager.prototype,

    /** @override */
     enabled: function() {
       return this.stateManager.isTrainingEnabled();
     },

    /** @override */
    updateListeners: function() {
      hotword.BaseSessionManager.prototype.updateListeners.call(this);

      if (this.enabled()) {
        // Detect when the speaker model needs to be finalized.
        if (!chrome.hotwordPrivate.onFinalizeSpeakerModel.hasListener(
                this.finalizedSpeakerModelListener_)) {
          chrome.hotwordPrivate.onFinalizeSpeakerModel.addListener(
              this.finalizedSpeakerModelListener_);
        }
        this.startSession(hotword.constants.RecognizerStartMode.NEW_MODEL);
      } else {
        chrome.hotwordPrivate.onFinalizeSpeakerModel.removeListener(
            this.finalizedSpeakerModelListener_);
      }
    },

    /** @override */
    handleHotwordTrigger: function(log) {
      if (this.enabled()) {
        hotword.BaseSessionManager.prototype.handleHotwordTrigger.call(
            this, log);
        this.startSession(hotword.constants.RecognizerStartMode.ADAPT_MODEL);
      }
    },

    /** @override */
    startSession: function(opt_mode) {
      this.stateManager.startSession(
          this.sessionSource_,
          function() {
            chrome.hotwordPrivate.setHotwordSessionState(true, function() {});
          },
          this.handleHotwordTrigger.bind(this),
          this.handleSpeakerModelSaved_.bind(this),
          opt_mode);
    },

    /**
     * Handles a hotwordPrivate.onFinalizeSpeakerModel event.
     * @private
     */
    handleFinalizeSpeakerModel_: function() {
      if (this.enabled())
        this.stateManager.finalizeSpeakerModel();
    },

    /**
     * Handles a hotwordPrivate.onFinalizeSpeakerModel event.
     * @private
     */
    handleSpeakerModelSaved_: function() {
      if (this.enabled())
        chrome.hotwordPrivate.notifySpeakerModelSaved();
    },
  };

  return {
    TrainingManager: TrainingManager
  };
});
