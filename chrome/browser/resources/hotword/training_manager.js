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
      if (this.enabled())
        this.startSession_();
    },

    /** @override */
    handleHotwordTrigger: function() {
      if (this.enabled()) {
        hotword.BaseSessionManager.prototype.handleHotwordTrigger.call(this);
        this.startSession_();
      }
    }
  };

  return {
    TrainingManager: TrainingManager
  };
});
