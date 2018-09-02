// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Namespace for Camera view.
 */
camera.views.camera = camera.views.camera || {};

/**
 * Creates a controller for the options of Camera view.
 * @param {camera.Router} router View router to switch views.
 * @param {function()} onNewStreamNeeded Callback called when new stream is
 *     needed for the changed options.
 * @constructor
 */
camera.views.camera.Options = function(router, onNewStreamNeeded) {
  /**
   * @type {camera.Router}
   * @private
   */
  this.router_ = router;

  /**
   * @type {function()}
   * @private
   */
  this.onNewStreamNeeded_ = onNewStreamNeeded;

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMic_ = document.querySelector('#toggle-mic');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMirror_ = document.querySelector('#toggle-mirror');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleGrid_ = document.querySelector('#toggle-grid');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleTimer_ = document.querySelector('#toggle-timer');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.toggleDevice_ = document.querySelector('#toggle-device');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.switchRecordVideo_ = document.querySelector('#switch-recordvideo');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.switchTakePhoto_ = document.querySelector('#switch-takephoto');

  /**
   * @type {Audio}
   * @private
   */
  this.shutterSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.tickSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.recordStartSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.recordEndSound_ = document.createElement('audio');

  /**
   * Device id of the camera device currently used or selected.
   * @type {?string}
   * @private
   */
  this.videoDeviceId_ = null;

  /**
   * Whether list of video devices is being refreshed now.
   * @type {boolean}
   * @private
   */
  this.refreshingVideoDeviceIds_ = false;

  /**
   * List of available video device ids.
   * @type {Promise<!Array<string>>}
   * @private
   */
  this.videoDeviceIds_ = null;

  /**
   * Mirroring set per device.
   * @type {Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Timeout to countdown a tick of the timer.
   * @type {?number}
   * @private
   */
  this.tickTimeout_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  this.switchRecordVideo_.addEventListener(
      'click', this.onSwitchRecordVideoClicked_.bind(this));
  this.switchTakePhoto_.addEventListener(
      'click', this.onSwitchTakePhotoClicked_.bind(this));
  this.toggleDevice_.addEventListener(
      'click', this.onToggleDeviceClicked_.bind(this));
  this.toggleMirror_.addEventListener(
      'click', this.onToggleMirrorClicked_.bind(this));
  this.toggleGrid_.addEventListener(
      'click', this.onToggleGridClicked_.bind(this));
  this.toggleTimer_.addEventListener(
      'click', this.onToggleTimerClicked_.bind(this));
  this.toggleMic_.addEventListener(
      'click', this.onToggleMicClicked_.bind(this));

  // Handle key-press for checkbox-type toggles.
  [this.toggleMirror_, this.toggleGrid_, this.toggleTimer_, this.toggleMic_]
      .forEach(element => {
    element.addEventListener('keypress', event => {
      if (camera.util.getShortcutIdentifier(event) == 'Enter') {
        element.click();
      }
    });
  });

  // Load the shutter, tick, and recording sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
  this.tickSound_.src = '../sounds/tick.ogg';
  this.recordStartSound_.src = '../sounds/record_start.ogg';
  this.recordEndSound_.src = '../sounds/record_end.ogg';
};

/**
 * Sounds.
 * @enum {number}
 */
camera.views.camera.Options.Sound = Object.freeze({
  SHUTTER: 0,
  TICK: 1,
  RECORDSTART: 2,
  RECORDEND: 3
});

/**
 * Prepares the options.
 */
camera.views.camera.Options.prototype.prepare = function() {
  // Show the mirror toggle on flipping-camera chromebooks (crbug.com/847737).
  camera.util.isChromeOSDevice(
      ['LENOVO-REKS1', 'LENOVO-REKS3', 'LENOVO-REKS4']).then(result => {
    this.toggleMirror_.hidden = !result;
  });

  // Select the default state of the toggle buttons.
  chrome.storage.local.get({
    toggleMic: true,
    toggleTimer: false,
    toggleGrid: false,
    mirroringToggles: {},  // Manually mirroring states per video device.
  }, values => {
    this.toggleMic_.checked = values.toggleMic;
    this.toggleTimer_.checked = values.toggleTimer;
    this.updateClass_(this.toggleTimer_, 'timer');
    this.toggleGrid_.checked = values.toggleGrid;
    this.updateClass_(this.toggleGrid_, 'grid');
    this.mirroringToggles_ = values.mirroringToggles;
  });
  // Remove the deprecated values.
  chrome.storage.local.remove(['effectIndex', 'toggleMulti', 'toggleMirror']);

  // TODO(yuli): Replace with devicechanged event.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(this.maybeRefreshVideoDeviceIds_.bind(this), 1000);
};

/**
 * Updates UI controls' disabled status for capturing/taking state changes.
 * @param {boolean} capturing Whether camera is capturing.
 * @param {boolean} capturing Whether camera is taking.
 */
camera.views.camera.Options.prototype.updateControls = function(
    capturing, taking) {
  var disabled = !capturing;
  this.toggleMirror_.disabled = disabled;
  this.toggleGrid_.disabled = disabled;
  this.toggleTimer_.disabled = disabled;
  this.toggleMic_.disabled = disabled;

  disabled = disabled || taking;
  this.toggleDevice_.disabled = disabled;
  this.switchRecordVideo_.disabled = disabled;
  this.switchTakePhoto_.disabled = disabled;
};

/**
 * Handles clicking on the video-recording switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onSwitchRecordVideoClicked_ = function(
    event) {
  document.body.classList.add('record-mode');
  this.onNewStreamNeeded_();
};

/**
 * Handles clicking on the photo-taking switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onSwitchTakePhotoClicked_ = function(
    event) {
  document.body.classList.remove('record-mode');
  this.onNewStreamNeeded_();
};

/**
 * Handles clicking on the toggle camera device switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleDeviceClicked_ = function(event) {
  this.toggleDevice_.classList.remove('animate');
  this.videoDeviceIds_.then(deviceIds => {
    var onTransitionEnd = (event) => {
      this.toggleDevice_.removeEventListener('transitionend', onTransitionEnd);
      this.toggleDevice_.classList.remove('animate');
    };
    this.toggleDevice_.addEventListener('transitionend', onTransitionEnd);
    this.toggleDevice_.offsetWidth;  // Force recalculation for animation.
    this.toggleDevice_.classList.add('animate');

    var index = deviceIds.indexOf(this.videoDeviceId_);
    if (index == -1) {
      index = 0;
    }
    if (deviceIds.length > 0) {
      index = (index + 1) % deviceIds.length;
      this.videoDeviceId_ = deviceIds[index];
    }
    this.onNewStreamNeeded_();
  });
};

/**
 * Updates a class attribute by a toggle's value.
 * @param {HTMLInputElement} toggle Element of a toggle.
 * @param {string} name Class name.
 * @return {boolean} Whether the updated result contains the class or not.
 * @private
 */
camera.views.camera.Options.prototype.updateClass_ = function(toggle, name) {
  return document.body.classList.toggle(name, toggle.checked);
};

/**
 * Handles clicking on the microphone switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleMicClicked_ = function(event) {
  chrome.storage.local.set({toggleMic: this.toggleMic_.checked});
  this.updateMicAudio();
};

/**
 * Handles clicking on the timer switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleTimerClicked_ = function(event) {
  chrome.storage.local.set(
      {toggleTimer: this.updateClass_(this.toggleTimer_, 'timer')});
};

/**
 * Handles clicking on the grid switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleGridClicked_ = function(event) {
  chrome.storage.local.set(
      {toggleGrid: this.updateClass_(this.toggleGrid_, 'grid')});
};

/**
 * Handles clicking on the mirror switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleMirrorClicked_ = function(event) {
  this.mirroringToggles_[this.videoDeviceId_] =
      this.updateClass_(this.toggleMirror_, 'mirror');
  chrome.storage.local.set({mirroringToggles: this.mirroringToggles_});
};

/**
 * Handles playing the sound by the speaker option.
 * @param {camera.views.camera.Options.Sound} sound Sound to be played.
 * @return {boolean} Whether the sound being played.
 */
camera.views.camera.Options.prototype.playSound = function(sound) {
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  var play = (element) => {
    element.currentTime = 0;
    element.play();
    return true;
  };
  switch (sound) {
    case camera.views.camera.Options.Sound.SHUTTER:
      return play(this.shutterSound_);
    case camera.views.camera.Options.Sound.TICK:
      return play(this.tickSound_);
    case camera.views.camera.Options.Sound.RECORDSTART:
      return play(this.recordStartSound_);
    case camera.views.camera.Options.Sound.RECORDEND:
      return play(this.recordEndSound_);
  }
};

/**
 * Handles enabling microphone audio by the microphone option.
 * @param {boolean} forceEnable Whether force to enable microphone.
 */
camera.views.camera.Options.prototype.updateMicAudio = function(forceEnable) {
  var enabled = forceEnable || this.toggleMic_.checked;
  if (this.toggleMic_.track) {
    this.toggleMic_.track.enabled = enabled;
  }
};

/**
 * Schedules ticks by the timer option if any.
 * @return {?Promise<>} Promise for the operation.
 */
camera.views.camera.Options.prototype.timerTicks = function() {
  if (!this.toggleTimer_.checked) {
    return null;
  }
  var cancel;
  var ticks = new Promise((resolve, reject) => {
    // TODO(yuli): Set tick-counter by timer settings.
    var tickCounter = 3;
    var onTimerTick = () => {
      if (tickCounter == 0) {
        resolve();
      } else {
        tickCounter--;
        this.tickTimeout_ = setTimeout(onTimerTick, 1000);
        this.playSound(camera.views.camera.Options.Sound.TICK);
        // Blink the toggle timer button.
        this.toggleTimer_.classList.add('animate');
        setTimeout(() => {
          this.toggleTimer_.classList.remove('animate');
        }, 500);
      }
    };
    // First tick immediately in the next message loop cycle.
    this.tickTimeout_ = setTimeout(onTimerTick, 0);
    cancel = reject;
  });

  ticks.cancel = () => {
    this.toggleTimer_.classList.remove('animate');
    if (this.tickTimeout_) {
      clearTimeout(this.tickTimeout_);
      this.tickTimeout_ = null;
    }
    cancel();
  };
  return ticks;
};

/**
 * Updates the options related to the stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 */
camera.views.camera.Options.prototype.updateStreamOptions = function(
    constraints, stream) {
  var track = stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  this.updateVideoDeviceId_(constraints, trackSettings);
  this.updateMirroring_(trackSettings);
  this.toggleMic_.track = stream.getAudioTracks()[0];
  this.updateMicAudio();
};

/**
 * Updates the video device id by the new stream.
 * @param {Object} constraints Stream constraints in use.
 * @param {MediaTrackSettings} trackSettings Video track settings in use.
 * @private
 */
camera.views.camera.Options.prototype.updateVideoDeviceId_ = function(
    constraints, trackSettings) {
  if (constraints.video.deviceId) {
    // For non-default cameras fetch the deviceId from constraints.
    // Works on all supported Chrome versions.
    this.videoDeviceId_ = constraints.video.deviceId.exact;
  } else {
    // For default camera, obtain the deviceId from settings, which is
    // a feature available only from 59. For older Chrome versions,
    // it's impossible to detect the device id. As a result, if the
    // default camera was changed to rear in chrome://settings, then
    // toggling the camera may not work when pressed for the first time
    // (the same camera would be opened).
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
  }
};

/**
 * Updates mirroring for a new stream.
 * @param {MediaTrackSettings} trackSettings Video track settings in use.
 * @private
 */
camera.views.camera.Options.prototype.updateMirroring_ = function(
    trackSettings) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
  var facingMode = trackSettings && trackSettings.facingMode;
  var enabled = facingMode ? facingMode == 'user' : true;

  // Override mirroring only if the mirror toggle is visible and mirroring was
  // set manually.
  if (!this.toggleMirror_.hidden &&
      this.videoDeviceId_ in this.mirroringToggles_) {
    enabled = this.mirroringToggles_[this.videoDeviceId_];
  }
  this.toggleMirror_.checked = enabled;
  this.updateClass_(this.toggleMirror_, 'mirror');
};

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
camera.views.camera.Options.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_) {
    return;
  }
  this.refreshingVideoDeviceIds_ = true;

  this.videoDeviceIds_ =
      navigator.mediaDevices.enumerateDevices().then(devices => {
    var availableVideoDevices = [];
    devices.forEach(device => {
      if (device.kind == 'videoinput') {
        availableVideoDevices.push(device.deviceId);
      }
    });
    return availableVideoDevices;
  });

  // Update UI options.
  this.videoDeviceIds_.then(deviceIds => {
    this.toggleDevice_.hidden = deviceIds.length < 2;
  }).catch(error => {
    console.error(error);
    this.toggleDevice_.hidden = true;
  }).finally(() => {
    this.refreshingVideoDeviceIds_ = false;
  });
};

/**
 * Gets the video device ids sorted by preference.
 * @return {!Promise<!Array<string>}
 */
camera.views.camera.Options.prototype.videoDeviceIds = function() {
  return this.videoDeviceIds_.then(deviceIds => {
    if (deviceIds.length == 0) {
      throw 'Device list empty.';
    }
    // Put the selected video device id first.
    var sorted = deviceIds.slice(0).sort((a, b) => {
      if (a == b) {
        return 0;
      }
      if (a == this.videoDeviceId_) {
        return -1;
      }
      return 1;
    });

    // Prepended 'null' deviceId means the system default camera. Add it only
    // when the app is launched (no video-device-id set).
    if (this.videoDeviceId_ == null) {
      sorted.unshift(null);
    }
    return sorted;
  });
};
