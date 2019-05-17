// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
 * Creates the camera-view controller.
 * @param {cca.models.Gallery} model Model object.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @constructor
 */
cca.views.Camera = function(model, resolBroker) {
  cca.views.View.call(this, '#camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * Layout handler for the camera view.
   * @type {cca.views.camera.Layout}
   * @private
   */
  this.layout_ = new cca.views.camera.Layout();

  /**
   * Video preview for the camera.
   * @type {cca.views.camera.Preview}
   * @private
   */
  this.preview_ = new cca.views.camera.Preview(this.stop_.bind(this));

  /**
   * Options for the camera.
   * @type {cca.views.camera.Options}
   * @private
   */
  this.options_ =
      new cca.views.camera.Options(resolBroker, this.stop_.bind(this));

  /**
   * Modes for the camera.
   * @type {cca.views.camera.Modes}
   * @private
   */
  this.modes_ = new cca.views.camera.Modes(
      resolBroker, this.stop_.bind(this), this.stop_.bind(this),
      async (blob, isMotionPicture, filename) => {
        if (blob) {
          cca.metrics.log(
              cca.metrics.Type.CAPTURE, this.facingMode_, blob.mins);
          try {
            await this.model_.savePicture(blob, isMotionPicture, filename);
          } catch (e) {
            cca.toast.show('error_msg_save_file_failed');
            throw e;
          }
        }
      });

  /**
   * @type {string}
   * @private
   */
  this.facingMode_ = '';

  /**
   * @type {boolean}
   * @private
   */
  this.locked_ = false;

  /**
   * @type {?number}
   * @private
   */
  this.retryStartTimeout_ = null;

  /**
   * Promise for the operation that starts camera.
   * @type {Promise}
   * @private
   */
  this.started_ = null;

  /**
   * Promise for the current take of photo or recording.
   * @type {?Promise}
   * @private
   */
  this.take_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelectorAll('#start-takephoto, #start-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.beginTake_()));

  document.querySelectorAll('#stop-takephoto, #stop-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.endTake_()));

  // Monitor the states to stop camera when locked/minimized.
  chrome.idle.onStateChanged.addListener((newState) => {
    this.locked_ = (newState == 'locked');
    if (this.locked_) {
      this.stop_();
    }
  });
  chrome.app.window.current().onMinimized.addListener(() => this.stop_());

  this.start_();
};

cca.views.Camera.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @override
 */
cca.views.Camera.prototype.focus = function() {
  // Avoid focusing invisible shutters.
  document.querySelectorAll('.shutter')
      .forEach((btn) => btn.offsetParent && btn.focus());
};


/**
 * Begins to take photo or recording with the current options, e.g. timer.
 * @private
 */
cca.views.Camera.prototype.beginTake_ = function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }

  cca.state.set('taking', true);
  this.focus();  // Refocus the visible shutter button for ChromeVox.
  this.take_ = (async () => {
    try {
      await cca.views.camera.timertick.start();

      await this.modes_.current.startCapture();
    } catch (e) {
      if (e && e.message == 'cancel') {
        return;
      }
      console.error(e);
    } finally {
      this.take_ = null;
      cca.state.set('taking', false);
      this.focus();  // Refocus the visible shutter button for ChromeVox.
    }
  })();
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.Camera.prototype.endTake_ = function() {
  cca.views.camera.timertick.cancel();
  this.modes_.current.stopCapture();
  return Promise.resolve(this.take_);
};

/**
 * @override
 */
cca.views.Camera.prototype.layout = function() {
  this.layout_.update();
};

/**
 * @override
 */
cca.views.Camera.prototype.handlingKey = function(key) {
  if (key == 'Ctrl-R') {
    cca.toast.show(this.preview_.toString());
    return true;
  }
  return false;
};

/**
 * Stops camera and tries to start camera stream again if possible.
 * @return {!Promise} Promise for the start-camera operation.
 * @private
 */
cca.views.Camera.prototype.stop_ = function() {
  // Wait for ongoing 'start' and 'capture' done before restarting camera.
  return Promise
      .all([
        this.started_,
        Promise.resolve(!cca.state.get('taking') || this.endTake_()),
      ])
      .finally(() => {
        this.preview_.stop();
        this.start_();
        return this.started_;
      });
};

/**
 * Try start stream reconfiguration with specified device id.
 * @async
 * @param {string} deviceId
 * @return {boolean} If found suitable stream and reconfigure successfully.
 */
cca.views.Camera.prototype.startWithDevice_ = async function(deviceId) {
  let supportedModes = null;
  for (const mode of this.modes_.getModeCandidates()) {
    const [photoRs, videoRs] =
        await this.options_.getDeviceResolutions(deviceId);
    for (const [[width, height], previewCandidates] of this.modes_
             .getResolutionCandidates(mode, deviceId, photoRs, videoRs)) {
      for (const constraints of previewCandidates) {
        try {
          const stream = await navigator.mediaDevices.getUserMedia(constraints);
          if (!supportedModes) {
            supportedModes = await this.modes_.getSupportedModes(stream);
            if (!supportedModes.includes(mode)) {
              stream.getTracks()[0].stop();
              return false;
            }
          }
          await this.preview_.start(stream);
          this.facingMode_ = this.options_.updateValues(constraints, stream);
          await this.modes_.updateModeSelectionUI(supportedModes);
          await this.modes_.updateMode(mode, stream, deviceId, width, height);
          cca.nav.close('warning', 'no-camera');
          return true;
        } catch (e) {
          this.preview_.stop();
          console.error(e);
        }
      }
    }
  }
  return false;
};

/**
 * Starts camera if the camera stream was stopped.
 * @private
 */
cca.views.Camera.prototype.start_ = function() {
  var suspend = this.locked_ || chrome.app.window.current().isMinimized();
  this.started_ =
      (async () => {
        if (!suspend) {
          for (const id of await this.options_.videoDeviceIds()) {
            if (await this.startWithDevice_(id)) {
              return;
            }
          }
        }
        throw new Error('suspend');
      })().catch((error) => {
        if (error && error.message != 'suspend') {
          console.error(error);
          cca.nav.open('warning', 'no-camera');
        }
        // Schedule to retry.
        if (this.retryStartTimeout_) {
          clearTimeout(this.retryStartTimeout_);
          this.retryStartTimeout_ = null;
        }
        this.retryStartTimeout_ = setTimeout(this.start_.bind(this), 100);
      });
};
