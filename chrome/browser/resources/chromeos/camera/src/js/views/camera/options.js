// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for the options of Camera view.
 * @param {function()} onNewStreamNeeded Callback to request new stream.
 * @constructor
 */
cca.views.camera.Options = function(onNewStreamNeeded) {
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
   * List of available video devices.
   * @type {Promise<!Array<MediaDeviceInfo>>}
   * @private
   */
  this.videoDevices_ = null;

  /**
   * Mirroring set per device.
   * @type {Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Current audio track in use.
   * @type {MediaStreamTrack}
   * @private
   */
  this.audioTrack_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  [
    ['#switch-device', () => this.switchDevice_()],
    ['#switch-recordvideo', () => this.switchMode_(true)],
    ['#switch-takephoto', () => this.switchMode_(false)],
    ['#toggle-grid', () => this.animatePreviewGrid_()],
    ['#open-settings', () => cca.nav.open('settings')],
  ].forEach(([selector, fn]) =>
      document.querySelector(selector).addEventListener('click', fn));

  this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
  this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

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
cca.views.camera.Options.Sound = Object.freeze({
  SHUTTER: 0,
  TICK: 1,
  RECORDSTART: 2,
  RECORDEND: 3,
});

cca.views.camera.Options.prototype = {
  get newStreamRequestDisabled() {
    return !document.body.classList.contains('capturing') ||
        document.body.classList.contains('taking');
  },
};

/**
 * Prepares the options.
 */
cca.views.camera.Options.prototype.prepare = function() {
  // Restore saved mirroring states per video device.
  chrome.storage.local.get({mirroringToggles: {}},
      (values) => this.mirroringToggles_ = values.mirroringToggles);
  // Remove the deprecated values.
  chrome.storage.local.remove(['effectIndex', 'toggleMulti', 'toggleMirror']);

  // TODO(yuli): Replace with devicechanged event.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(this.maybeRefreshVideoDeviceIds_.bind(this), 1000);
};

/**
 * Switches mode to either video-recording or photo-taking.
 * @param {boolean} record True for record-mode, false otherwise.
 * @private
 */
cca.views.camera.Options.prototype.switchMode_ = function(record) {
  if (this.newStreamRequestDisabled) {
    return;
  }
  document.body.classList.toggle('record-mode', record);
  document.body.classList.add('mode-switching');
  this.onNewStreamNeeded_().then(
      () => document.body.classList.remove('mode-switching'));
};

/**
 * Switches to the next available camera device.
 * @private
 */
cca.views.camera.Options.prototype.switchDevice_ = function() {
  if (this.newStreamRequestDisabled) {
    return;
  }
  this.videoDevices_.then((devices) => {
    cca.util.animateOnce(document.querySelector('#switch-device'));
    var index = devices.findIndex(
        (entry) => entry.deviceId == this.videoDeviceId_);
    if (index == -1) {
      index = 0;
    }
    if (devices.length > 0) {
      index = (index + 1) % devices.length;
      this.videoDeviceId_ = devices[index].deviceId;
    }
    return this.onNewStreamNeeded_();
  }).then(() => this.videoDevices_).then((devices) => {
    // Make the active camera announced by screen reader.
    var found = devices.find((entry) => entry.deviceId == this.videoDeviceId_);
    if (found) {
      cca.toast.speak(chrome.i18n.getMessage(
          'statusMsgCameraSwitched', found.label));
    }
  });
};

/**
 * Animates the preview grid.
 * @private
 */
cca.views.camera.Options.prototype.animatePreviewGrid_ = function() {
  Array.from(document.querySelector('#preview-grid').children).forEach(
      (grid) => cca.util.animateOnce(grid));
};

/**
 * Handles playing the sound by the speaker option.
 * @param {cca.views.camera.Options.Sound} sound Sound to be played.
 * @return {boolean} Whether the sound being played.
 * TODO(yuli): Move this function into sounds.js.
 */
cca.views.camera.Options.prototype.playSound = function(sound) {
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  var play = (element) => {
    element.currentTime = 0;
    element.play();
    return true;
  };
  switch (sound) {
    case cca.views.camera.Options.Sound.SHUTTER:
      return play(this.shutterSound_);
    case cca.views.camera.Options.Sound.TICK:
      return play(this.tickSound_);
    case cca.views.camera.Options.Sound.RECORDSTART:
      return play(this.recordStartSound_);
    case cca.views.camera.Options.Sound.RECORDEND:
      return play(this.recordEndSound_);
  }
};

/**
 * Schedules ticks by the timer option if any.
 * @return {?Promise} Promise for the operation.
 * TODO(yuli): Move this function into timerticks.js.
 */
cca.views.camera.Options.prototype.timerTicks = function() {
  if (!document.body.classList.contains('timer')) {
    return null;
  }
  var cancel;
  var tickTimeout = null;
  var tickMsg = document.querySelector('#timer-tick-msg');
  var ticks = new Promise((resolve, reject) => {
    var tickCounter = document.body.classList.contains('_10sec') ? 10 : 3;
    var onTimerTick = () => {
      if (tickCounter == 0) {
        resolve();
      } else {
        this.playSound(cca.views.camera.Options.Sound.TICK);
        tickMsg.textContent = tickCounter + '';
        cca.util.animateOnce(tickMsg);
        tickTimeout = setTimeout(onTimerTick, 1000);
        tickCounter--;
      }
    };
    // First tick immediately in the next message loop cycle.
    tickTimeout = setTimeout(onTimerTick, 0);
    cancel = reject;
  });

  ticks.cancel = () => {
    if (tickTimeout) {
      clearTimeout(tickTimeout);
      tickTimeout = null;
    }
    cca.util.animateCancel(tickMsg);
    cancel();
  };
  return ticks;
};

/**
 * Updates the options' values for the current constraints and stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 */
cca.views.camera.Options.prototype.updateValues = function(
    constraints, stream) {
  var track = stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  this.updateVideoDeviceId_(constraints, trackSettings);
  this.updateMirroring_(trackSettings);
  this.audioTrack_ = stream.getAudioTracks()[0];
  this.updateAudioByMic_();
};

/**
 * Updates the video device id by the new stream.
 * @param {Object} constraints Stream constraints in use.
 * @param {MediaTrackSettings} trackSettings Video track settings in use.
 * @private
 */
cca.views.camera.Options.prototype.updateVideoDeviceId_ = function(
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
cca.views.camera.Options.prototype.updateMirroring_ = function(trackSettings) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
  var facingMode = trackSettings && trackSettings.facingMode;
  var enabled = facingMode ? facingMode == 'user' : true;

  // Override mirroring only if mirroring was toggled manually.
  if (this.videoDeviceId_ in this.mirroringToggles_) {
    enabled = this.mirroringToggles_[this.videoDeviceId_];
  }
  this.toggleMirror_.toggleChecked(enabled);
};

/**
 * Saves the toggled mirror state for the current video device.
 * @private
 */
cca.views.camera.Options.prototype.saveMirroring_ = function() {
  this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
  chrome.storage.local.set({mirroringToggles: this.mirroringToggles_});
};

/**
 * Enables/disables the current audio track by the microphone option.
 * @private
 */
cca.views.camera.Options.prototype.updateAudioByMic_ = function() {
  if (this.audioTrack_) {
    this.audioTrack_.enabled = this.toggleMic_.checked;
  }
};

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
cca.views.camera.Options.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_) {
    return;
  }
  this.refreshingVideoDeviceIds_ = true;

  this.videoDevices_ = navigator.mediaDevices.enumerateDevices().then(
      (devices) => devices.filter((device) => device.kind == 'videoinput'));

  var multi = false;
  this.videoDevices_.then((devices) => {
    multi = devices.length >= 2;
  }).catch(console.error).finally(() => {
    document.body.classList.toggle('multi-camera', multi);
    this.refreshingVideoDeviceIds_ = false;
  });
};

/**
 * Gets the video device ids sorted by preference.
 * @return {!Promise<!Array<string>>}
 */
cca.views.camera.Options.prototype.videoDeviceIds = function() {
  return this.videoDevices_.then((devices) => {
    if (devices.length == 0) {
      throw new Error('Device list empty.');
    }
    // Put the selected video device id first.
    var sorted = devices.map((device) => device.deviceId).sort((a, b) => {
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
