// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class to manage hotwording state. Starts/stops the hotword detector based
   * on user settings, session requests, and any other factors that play into
   * whether or not hotwording should be running.
   * @constructor
   * @struct
   */
  function StateManager() {
    /**
     * Current state.
     * @private {hotword.StateManager.State_}
     */
    this.state_ = State_.STOPPED;

    /**
     * Current hotwording status.
     * @private {?chrome.hotwordPrivate.StatusDetails}
     */
    this.hotwordStatus_ = null;

    /**
     * NaCl plugin manager.
     * @private {?hotword.NaClManager}
     */
    this.pluginManager_ = null;

    // Get the initial status.
    chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));
  }

  /**
   * @enum {number}
   * @private
   */
  StateManager.State_ = {
    STOPPED: 0,
    STARTING: 1,
    RUNNING: 2,
    ERROR: 3,
  };
  var State_ = StateManager.State_;

  StateManager.prototype = {
    /**
     * Request status details update. Intended to be called from the
     * hotwordPrivate.onEnabledChanged() event.
     */
    updateStatus: function() {
      chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));
    },

    /**
     * Callback for hotwordPrivate.getStatus() function.
     * @param {chrome.hotwordPrivate.StatusDetails} status Current hotword
     *     status.
     * @private
     */
    handleStatus_: function(status) {
      this.hotwordStatus_ = status;
      this.updateStateFromStatus_();
    },

    /**
     * Updates state based on the current status.
     * @private
     */
    updateStateFromStatus_: function() {
      if (this.hotwordStatus_.enabled) {
        // Hotwording is enabled.
        // TODO(amistry): Have a separate alwaysOnEnabled flag. For now, treat
        // "enabled" as "always on enabled".
        this.startDetector_();
      } else {
        // Not enabled. Shut down if running.
        this.shutdownDetector_();
      }
    },

    /**
     * Starts the hotword detector.
     * @private
     */
    startDetector_: function() {
      // Last attempt to start detector resulted in an error.
      if (this.state_ == State_.ERROR) {
        // TODO(amistry): Do some error rate tracking here and disable the
        // extension if we error too often.
      }

      if (!this.pluginManager_) {
        this.state_ = State_.STARTING;
        this.pluginManager_ = new hotword.NaClManager();
        this.pluginManager_.addEventListener(hotword.constants.Event.READY,
                                             this.onReady_.bind(this));
        this.pluginManager_.addEventListener(hotword.constants.Event.ERROR,
                                             this.onError_.bind(this));
        this.pluginManager_.addEventListener(hotword.constants.Event.TRIGGER,
                                             this.onTrigger_.bind(this));
        chrome.runtime.getPlatformInfo(function(platform) {
          var naclArch = platform.nacl_arch;

          // googDucking set to false so that audio output level from other tabs
          // is not affected when hotword is enabled. https://crbug.com/357773
          // content/common/media/media_stream_options.cc
          var constraints = /** @type {googMediaStreamConstraints} */
              ({audio: {optional: [{googDucking: false}]}});
          navigator.webkitGetUserMedia(
              /** @type {MediaStreamConstraints} */ (constraints),
              function(stream) {
                if (!this.pluginManager_.initialize(naclArch, stream)) {
                  this.state_ = State_.ERROR;
                  this.shutdownPluginManager_();
                }
              }.bind(this),
              function(error) {
                this.state_ = State_.ERROR;
                this.pluginManager_ = null;
              }.bind(this));
        }.bind(this));
      } else if (this.state_ != State_.STARTING) {
        // Don't try to start a starting detector.
        this.state_ = State_.RUNNING;
        this.pluginManager_.startRecognizer();
      }
    },

    /**
     * Shuts down and removes the plugin manager, if it exists.
     * @private
     */
    shutdownPluginManager_: function() {
      if (this.pluginManager_) {
        this.pluginManager_.shutdown();
        this.pluginManager_ = null;
      }
    },

    /**
     * Shuts down the hotword detector.
     * @private
     */
    shutdownDetector_: function() {
      this.state_ = State_.STOPPED;
      this.shutdownPluginManager_();
    },

    /**
     * Handle the hotword plugin being ready to start.
     * @private
     */
    onReady_: function() {
      if (this.state_ != State_.STARTING) {
        // At this point, we should not be in the RUNNING state. Doing so would
        // imply the hotword detector was started without being ready.
        assert(this.state_ != State_.RUNNING);
        this.shutdownPluginManager_();
        return;
      }
      this.state_ = State_.RUNNING;
      this.pluginManager_.startRecognizer();
    },

    /**
     * Handle an error from the hotword plugin.
     * @private
     */
    onError_: function() {
      this.state_ = State_.ERROR;
      this.shutdownPluginManager_();
    },

    /**
     * Handle hotword triggering.
     * @private
     */
    onTrigger_: function() {
      assert(this.pluginManager_);
      // Detector implicitly stops when the hotword is detected.
      this.state_ = State_.STOPPED;

      chrome.hotwordPrivate.notifyHotwordRecognition('search', function() {});
    }
  };

  return {
    StateManager: StateManager
  };
});
