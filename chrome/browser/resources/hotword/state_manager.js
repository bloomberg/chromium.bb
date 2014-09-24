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

    /**
     * Source of the current hotword session.
     * @private {?hotword.constants.SessionSource}
     */
    this.sessionSource_ = null;

    /**
     * Callback to run when the hotword detector has successfully started.
     * @private {!function()}
     */
    this.sessionStartedCb_ = null;

    /**
     * Hotword trigger audio notification... a.k.a The Chime (tm).
     * @private {!Audio}
     */
    this.chime_ = document.createElement('audio');

    // Get the initial status.
    chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));

    // Setup the chime and insert into the page.
    this.chime_.src = chrome.extension.getURL(
        hotword.constants.SHARED_MODULE_ROOT + '/audio/chime.wav');
    document.body.appendChild(this.chime_);
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
      if (!this.hotwordStatus_)
        return;

      if (this.hotwordStatus_.enabled) {
        // Start the detector if there's a session, and shut it down if there
        // isn't.
        // TODO(amistry): Support stacking sessions. This can happen when the
        // user opens google.com or the NTP, then opens the launcher. Opening
        // google.com will create one session, and opening the launcher will
        // create the second. Closing the launcher should re-activate the
        // google.com session.
        // NOTE(amistry): With always-on, we want a different behaviour with
        // sessions since the detector should always be running. The exception
        // being when the user triggers by saying 'Ok Google'. In that case, the
        // detector stops, so starting/stopping the launcher session should
        // restart the detector.
        if (this.sessionSource_)
          this.startDetector_();
        else
          this.shutdownDetector_();
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
        this.startRecognizer_();
      }
    },

    /**
     * Start the recognizer plugin. Assumes the plugin has been loaded and is
     * ready to start.
     * @private
     */
    startRecognizer_: function() {
      assert(this.pluginManager_);
      if (this.state_ != State_.RUNNING) {
        this.state_ = State_.RUNNING;
        this.pluginManager_.startRecognizer();
      }
      if (this.sessionStartedCb_) {
        this.sessionStartedCb_();
        this.sessionStartedCb_ = null;
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
      this.startRecognizer_();
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

      // Play the chime.
      this.chime_.play();

      chrome.hotwordPrivate.notifyHotwordRecognition('search', function() {});

      // Implicitly clear the session. A session needs to be started in order to
      // restart the detector.
      this.sessionSource_ = null;
      this.sessionStartedCb_ = null;
    },

    /**
     * Start a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     * @param {!function()} startedCb Callback invoked when the session has
     *     been started successfully.
     */
    startSession: function(source, startedCb) {
      this.sessionSource_ = source;
      this.sessionStartedCb_ = startedCb;
      this.updateStateFromStatus_();
    },

    /**
     * Stops a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     */
    stopSession: function(source) {
      this.sessionSource_ = null;
      this.sessionStartedCb_ = null;
      this.updateStateFromStatus_();
    }
  };

  return {
    StateManager: StateManager
  };
});
