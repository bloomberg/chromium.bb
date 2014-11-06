// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Trivial container class for session information.
   * @param {!hotword.constants.SessionSource} source Source of the hotword
   *     session.
   * @param {!function()} triggerCb Callback invoked when the hotword has
   *     triggered.
   * @param {!function()} startedCb Callback invoked when the session has
   *     been started successfully.
   * @constructor
   * @struct
   * @private
   */
  function Session_(source, triggerCb, startedCb) {
    /**
     * Source of the hotword session request.
     * @private {!hotword.constants.SessionSource}
     */
    this.source_ = source;

     /**
      * Callback invoked when the hotword has triggered.
      * @private {!function()}
      */
    this.triggerCb_ = triggerCb;

    /**
     * Callback invoked when the session has been started successfully.
     * @private {?function()}
     */
    this.startedCb_ = startedCb;
  }

  /**
   * Class to manage hotwording state. Starts/stops the hotword detector based
   * on user settings, session requests, and any other factors that play into
   * whether or not hotwording should be running.
   * @constructor
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
     * Currently active hotwording sessions.
     * @private {!Array.<Session_>}
     */
    this.sessions_ = [];

    /**
     * Event that fires when the hotwording status has changed.
     * @type {!ChromeEvent}
     */
    this.onStatusChanged = new chrome.Event();

    /**
     * Hotword trigger audio notification... a.k.a The Chime (tm).
     * @private {!HTMLAudioElement}
     */
    this.chime_ =
        /** @type {!HTMLAudioElement} */(document.createElement('audio'));

    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     * @private
     */
    this.idleStateChangedListener_ = this.handleIdleStateChanged_.bind(this);

    /**
     * Whether this user is locked.
     * @private {boolean}
     */
    this.isLocked_ = false;

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

  var UmaMediaStreamOpenResults_ = {
    // These first error are defined by the MediaStream spec:
    // http://w3c.github.io/mediacapture-main/getusermedia.html#idl-def-MediaStreamError
    'NotSupportedError':
        hotword.constants.UmaMediaStreamOpenResult.NOT_SUPPORTED,
    'PermissionDeniedError':
        hotword.constants.UmaMediaStreamOpenResult.PERMISSION_DENIED,
    'ConstraintNotSatisfiedError':
        hotword.constants.UmaMediaStreamOpenResult.CONSTRAINT_NOT_SATISFIED,
    'OverconstrainedError':
        hotword.constants.UmaMediaStreamOpenResult.OVERCONSTRAINED,
    'NotFoundError': hotword.constants.UmaMediaStreamOpenResult.NOT_FOUND,
    'AbortError': hotword.constants.UmaMediaStreamOpenResult.ABORT,
    'SourceUnavailableError':
        hotword.constants.UmaMediaStreamOpenResult.SOURCE_UNAVAILABLE,
    // The next few errors are chrome-specific. See:
    // content/renderer/media/user_media_client_impl.cc
    // (UserMediaClientImpl::GetUserMediaRequestFailed)
    'PermissionDismissedError':
        hotword.constants.UmaMediaStreamOpenResult.PERMISSION_DISMISSED,
    'InvalidStateError':
        hotword.constants.UmaMediaStreamOpenResult.INVALID_STATE,
    'DevicesNotFoundError':
        hotword.constants.UmaMediaStreamOpenResult.DEVICES_NOT_FOUND,
    'InvalidSecurityOriginError':
        hotword.constants.UmaMediaStreamOpenResult.INVALID_SECURITY_ORIGIN
  };

  StateManager.prototype = {
    /**
     * Request status details update. Intended to be called from the
     * hotwordPrivate.onEnabledChanged() event.
     */
    updateStatus: function() {
      chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));
    },

    /**
     * @return {boolean} True if google.com/NTP/launcher hotwording is enabled.
     */
    isSometimesOnEnabled: function() {
      assert(this.hotwordStatus_,
             'No hotwording status (isSometimesOnEnabled)');
      // Although the two settings are supposed to be mutually exclusive, it's
      // possible for both to be set. In that case, always-on takes precedence.
      return this.hotwordStatus_.enabled &&
          !this.hotwordStatus_.alwaysOnEnabled;
    },

    /**
     * @return {boolean} True if always-on hotwording is enabled.
     */
    isAlwaysOnEnabled: function() {
      assert(this.hotwordStatus_, 'No hotword status (isAlwaysOnEnabled)');
      return this.hotwordStatus_.alwaysOnEnabled;
    },

    /**
     * @return {boolean} True if training is enabled.
     */
    isTrainingEnabled: function() {
      assert(this.hotwordStatus_, 'No hotword status (isTrainingEnabled)');
      return this.hotwordStatus_.trainingEnabled;
    },

    /**
     * Callback for hotwordPrivate.getStatus() function.
     * @param {chrome.hotwordPrivate.StatusDetails} status Current hotword
     *     status.
     * @private
     */
    handleStatus_: function(status) {
      hotword.debug('New hotword status', status);
      this.hotwordStatus_ = status;
      this.updateStateFromStatus_();

      this.onStatusChanged.dispatch();
    },

    /**
     * Updates state based on the current status.
     * @private
     */
    updateStateFromStatus_: function() {
      if (!this.hotwordStatus_)
        return;

      if (this.hotwordStatus_.enabled ||
          this.hotwordStatus_.alwaysOnEnabled ||
          this.hotwordStatus_.trainingEnabled) {
        // Start the detector if there's a session and the user is unlocked, and
        // shut it down otherwise.
        if (this.sessions_.length && !this.isLocked_)
          this.startDetector_();
        else
          this.shutdownDetector_();

        if (!chrome.idle.onStateChanged.hasListener(
                this.idleStateChangedListener_)) {
          chrome.idle.onStateChanged.addListener(
              this.idleStateChangedListener_);
        }
      } else {
        // Not enabled. Shut down if running.
        this.shutdownDetector_();

        chrome.idle.onStateChanged.removeListener(
            this.idleStateChangedListener_);
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
                hotword.metrics.recordEnum(
                    hotword.constants.UmaMetrics.MEDIA_STREAM_RESULT,
                    hotword.constants.UmaMediaStreamOpenResult.SUCCESS,
                    hotword.constants.UmaMediaStreamOpenResult.MAX);
                if (!this.pluginManager_.initialize(naclArch, stream)) {
                  this.state_ = State_.ERROR;
                  this.shutdownPluginManager_();
                }
              }.bind(this),
              function(error) {
                if (error.name in UmaMediaStreamOpenResults_) {
                  var metricValue = UmaMediaStreamOpenResults_[error.name];
                } else {
                  var metricValue =
                      hotword.constants.UmaMediaStreamOpenResult.UNKNOWN;
                }
                hotword.metrics.recordEnum(
                    hotword.constants.UmaMetrics.MEDIA_STREAM_RESULT,
                    metricValue,
                    hotword.constants.UmaMediaStreamOpenResult.MAX);
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
      assert(this.pluginManager_, 'No NaCl plugin loaded');
      if (this.state_ != State_.RUNNING) {
        this.state_ = State_.RUNNING;
        this.pluginManager_.startRecognizer();
      }
      for (var i = 0; i < this.sessions_.length; i++) {
        var session = this.sessions_[i];
        if (session.startedCb_) {
          session.startedCb_();
          session.startedCb_ = null;
        }
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
        assert(this.state_ != State_.RUNNING, 'Unexpected RUNNING state');
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
      hotword.debug('Hotword triggered!');
      chrome.metricsPrivate.recordUserAction(
          hotword.constants.UmaMetrics.TRIGGER);
      assert(this.pluginManager_, 'No NaCl plugin loaded on trigger');
      // Detector implicitly stops when the hotword is detected.
      this.state_ = State_.STOPPED;

      // Play the chime.
      this.chime_.play();

      // Implicitly clear the top session. A session needs to be started in
      // order to restart the detector.
      if (this.sessions_.length) {
        var session = this.sessions_.pop();
        if (session.triggerCb_)
          session.triggerCb_();
      }
    },

    /**
     * Remove a hotwording session from the given source.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     * @private
     */
    removeSession_: function(source) {
      for (var i = 0; i < this.sessions_.length; i++) {
        if (this.sessions_[i].source_ == source) {
          this.sessions_.splice(i, 1);
          break;
        }
      }
    },

    /**
     * Start a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     * @param {!function()} startedCb Callback invoked when the session has
     *     been started successfully.
     * @param {!function()} triggerCb Callback invoked when the hotword has
     *     triggered.
     */
    startSession: function(source, startedCb, triggerCb) {
      hotword.debug('Starting session for source: ' + source);
      this.removeSession_(source);
      this.sessions_.push(new Session_(source, triggerCb, startedCb));
      this.updateStateFromStatus_();
    },

    /**
     * Stops a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     */
    stopSession: function(source) {
      hotword.debug('Stopping session for source: ' + source);
      this.removeSession_(source);
      this.updateStateFromStatus_();
    },

    /**
     * Handles a chrome.idle.onStateChanged event.
     * @param {!string} state State, one of "active", "idle", or "locked".
     * @private
     */
    handleIdleStateChanged_: function(state) {
      hotword.debug('Idle state changed: ' + state);
      var oldLocked = this.isLocked_;
      if (state == 'locked')
        this.isLocked_ = true;
      else
        this.isLocked_ = false;

      if (oldLocked != this.isLocked_)
        this.updateStateFromStatus_();
    }
  };

  return {
    StateManager: StateManager
  };
});
