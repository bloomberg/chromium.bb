// Copyright 2018 The Chromium Authors. All rights reserved.
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
 * @param {cca.views.camera.PhotoResolPreferrer} photoResolPreferrer
 * @param {cca.views.camera.VideoResolPreferrer} videoResolPreferrer
 * @param {function()} doSwitchDevice Callback to trigger device switching.
 * @constructor
 */
cca.views.camera.Options = function(
    photoResolPreferrer, videoResolPreferrer, doSwitchDevice) {
  /**
   * @type {cca.views.camera.PhotoResolPreferrer}
   * @private
   */
  this.photoResolPreferrer_ = photoResolPreferrer;

  /**
   * @type {cca.views.camera.VideoResolPreferrer}
   * @private
   */
  this.videoResolPreferrer_ = videoResolPreferrer;

  /**
   * @type {function()}
   * @private
   */
  this.doSwitchDevice_ = doSwitchDevice;

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
   * List of available video devices and width, height of its supported video
   * resolutions and photo resolutions.
   * @type {Promise<!Array<[MediaDeviceInfo, cros.mojom.CameraFacing, ResolList,
   *     ResolList]>>}
   * @private
   */
  this.deviceResolutions_ = null;

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

  [['#switch-device', () => this.switchDevice_()],
   ['#toggle-grid', () => this.animatePreviewGrid_()],
   ['#open-settings', () => cca.nav.open('settings')],
  ]
      .forEach(
          ([selector, fn]) =>
              document.querySelector(selector).addEventListener('click', fn));

  this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
  this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

  // Restore saved mirroring states per video device.
  chrome.storage.local.get({mirroringToggles: {}},
      (values) => this.mirroringToggles_ = values.mirroringToggles);
  // Remove the deprecated values.
  chrome.storage.local.remove(['effectIndex', 'toggleMulti', 'toggleMirror']);

  // TODO(yuli): Replace with devicechanged event.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(() => this.maybeRefreshVideoDeviceIds_(), 1000);
};

/**
 * Switches to the next available camera device.
 * @private
 */
cca.views.camera.Options.prototype.switchDevice_ = function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
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
    return this.doSwitchDevice_();
  }).then(() => this.videoDevices_).then((devices) => {
    // Make the active camera announced by screen reader.
    var found = devices.find((entry) => entry.deviceId == this.videoDeviceId_);
    if (found) {
      cca.toast.speak(chrome.i18n.getMessage(
          'status_msg_camera_switched', found.label));
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
 * Updates the options' values for the current constraints and stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 * @return {string} Facing-mode in use.
 */
cca.views.camera.Options.prototype.updateValues = function(
    constraints, stream) {
  var track = stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  var facingMode = trackSettings && trackSettings.facingMode;
  this.updateVideoDeviceId_(constraints, trackSettings);
  this.updateMirroring_(facingMode);
  this.audioTrack_ = stream.getAudioTracks()[0];
  this.updateAudioByMic_();
  return facingMode;
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
 * @param {string} facingMode Facing-mode of the stream.
 * @private
 */
cca.views.camera.Options.prototype.updateMirroring_ = function(facingMode) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
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
    cca.state.set('multi-camera', multi);
    this.refreshingVideoDeviceIds_ = false;
  });

  this.deviceResolutions_ =
      this.videoDevices_
          .then((devices) => {
            return Promise.all(devices.map((d) => Promise.all([
              d,
              cca.mojo.getCameraFacing(d.deviceId),
              cca.mojo.getPhotoResolutions(d.deviceId),
              cca.mojo.getVideoConfigs(d.deviceId)
                  .then(
                      (v) => v.filter(([, , fps]) => fps >= 24)
                                 .map(([w, h]) => [w, h])),
            ])));
          })
          .catch((e) => {
            cca.state.set('no-resolution-settings', true);
            throw e;
          });

  (async () => {
    try {
      var deviceResolutions = await this.deviceResolutions_;
    } catch (e) {
      return;
    }
    let frontSetting = null;
    let backSetting = null;
    let externalSettings = [];
    deviceResolutions.forEach(([{deviceId}, facing, photoRs, videoRs]) => {
      const setting = [deviceId, photoRs, videoRs];
      switch (facing) {
        case cros.mojom.CameraFacing.CAMERA_FACING_FRONT:
          frontSetting = setting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_BACK:
          backSetting = setting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_EXTERNAL:
          externalSettings.push(setting);
          break;
        default:
          console.error(`Ignore device of unknown facing: ${facing}`);
      }
    });
    this.photoResolPreferrer_.updateResolutions(
        frontSetting && [frontSetting[0], frontSetting[1]],
        backSetting && [backSetting[0], backSetting[1]],
        externalSettings.map(([deviceId, photoRs]) => [deviceId, photoRs]));
    this.videoResolPreferrer_.updateResolutions(
        frontSetting && [frontSetting[0], frontSetting[2]],
        backSetting && [backSetting[0], backSetting[2]],
        externalSettings.map(([deviceId, , videoRs]) => [deviceId, videoRs]));
  })();
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
    return devices
        .sort((a, b) => {
          if (a.deviceId == b.deviceId) {
            return 0;
          }
          if (this.videoDeviceId_ ?
                  (a.deviceId == this.videoDeviceId_) :
                  (a.label == cca.views.camera.Options.FRONT_CAMERA_LABEL)) {
            return -1;
          }
          return 1;
        })
        .map(({deviceId}) => deviceId);
  });
};

/**
 * Gets supported photo and video resolutions for specified video device.
 * @async
 * @param {string} deviceId Device id of the video device.
 * @return {[ResolList, ResolList]} Supported photo and video resolutions.
 * @throws {Error} May fail on HALv1 device without capability of querying
 *     supported resolutions.
 */
cca.views.camera.Options.prototype.getDeviceResolutions =
    async function(deviceId) {
  const deviceResolutions = await this.deviceResolutions_;
  const [, , photoRs, videoRs] =
      deviceResolutions.find(([d]) => d.deviceId == deviceId);
  return [photoRs, videoRs];
};
