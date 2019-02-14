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
 * @constructor
 */
cca.views.Camera = function(model) {
  cca.views.View.call(this, '#camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * MediaRecorder object to record motion pictures.
   * @type {MediaRecorder}
   * @private
   */
  this.mediaRecorder_ = null;

  /**
   * ImageCapture object to capture still photos.
   * @type {ImageCapture}
   * @private
   */
  this.imageCapture_ = null;

  /**
   * Promise that gets the photo capabilities of the current image-capture.
   * @type {Promise<PhotoCapabilities>}
   * @private
   */
  this.photoCapabilities_ = null;

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
  this.options_ = new cca.views.camera.Options(this.stop_.bind(this));

  /**
   * Record-time for the elapsed recording time.
   * @type {cca.views.camera.RecordTime}
   * @private
   */
  this.recordTime_ = new cca.views.camera.RecordTime();

  /**
   * Button for taking photos and recording videos.
   * @type {HTMLButtonElement}
   * @private
   */
  this.shutterButton_ = document.querySelector('#shutter');

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
   * Timeout for a take of photo or recording.
   * @type {?number}
   * @private
   */
  this.takeTimeout_ = null;

  /**
   * Promise for the current take of photo or recording.
   * @type {Promise<Blob>}
   * @private
   */
  this.take_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  this.shutterButton_.addEventListener('click',
      () => this.onShutterButtonClicked_());

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

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred format.
 * @type {string}
 * @const
 */
cca.views.Camera.RECORD_MIMETYPE = 'video/x-matroska;codecs=avc1';

cca.views.Camera.prototype = {
  __proto__: cca.views.View.prototype,
  get recordMode() {
    return cca.state.get('record-mode');
  },
};

/**
 * @override
 */
cca.views.Camera.prototype.focus = function() {
  this.shutterButton_.focus();
};

/**
 * Handles clicking on the shutter button.
 * @param {Event} event Mouse event
 * @private
 */
cca.views.Camera.prototype.onShutterButtonClicked_ = function(event) {
  if (!cca.state.get('streaming')) {
    return;
  }
  if (cca.state.get('taking')) {
    // End the prior ongoing take if any; a new take shouldn't be started
    // until the prior one is ended.
    this.endTake_();
    return;
  }
  try {
    if (this.recordMode) {
      this.prepareMediaRecorder_();
    } else {
      this.prepareImageCapture_();
    }
    this.beginTake_();
  } catch (e) {
    console.error(e);
    cca.toast.show(this.recordMode ?
        'error_msg_record_start_failed' : 'error_msg_take_photo_failed');
  }
};

/**
 * Updates the shutter button's label for taking/record-mode state changes.
 * @private
 */
cca.views.Camera.prototype.updateShutterLabel_ = function() {
  var label;
  if (this.recordMode) {
    label = cca.state.get('taking') ?
        'record_video_stop_button' : 'record_video_start_button';
  } else {
    label = (cca.state.get('taking') && cca.state.get('timer')) ?
        'take_photo_cancel_button' : 'take_photo_button';
  }
  this.shutterButton_.setAttribute('aria-label', chrome.i18n.getMessage(label));
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
 * Begins to take photo or recording with the current options, e.g. timer.
 * @private
 */
cca.views.Camera.prototype.beginTake_ = function() {
  cca.state.set('taking', true);
  this.updateShutterLabel_();

  cca.views.camera.timertick.start().then(() => {
    // Play a sound before starting to record and delay the take to avoid the
    // sound being recorded if necessary.
    var delay =
        (this.recordMode && cca.sound.play('#sound-rec-start')) ? 250 : 0;
    this.takeTimeout_ = setTimeout(() => {
      if (this.recordMode) {
        // Take of recording will be ended by another shutter click.
        this.take_ = this.createRecordingBlob_().catch((error) => {
          cca.toast.show('error_msg_empty_recording');
          throw error;
        });
      } else {
        this.take_ = this.createPhotoBlob_().catch((error) => {
          cca.toast.show('error_msg_take_photo_failed');
          throw error;
        });
        this.endTake_();
      }
    }, delay);
  }).catch(() => {});
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.Camera.prototype.endTake_ = function() {
  cca.views.camera.timertick.cancel();
  if (this.takeTimeout_) {
    clearTimeout(this.takeTimeout_);
    this.takeTimeout_ = null;
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }

  return Promise.resolve(this.take_).then((blob) => {
    if (blob && !blob.handled) {
      // Play a sound and save the result after a successful take.
      blob.handled = true;
      var recordMode = this.recordMode;
      cca.sound.play(recordMode ? '#sound-rec-end' : '#sound-shutter');
      return this.model_.savePicture(blob, recordMode).catch((error) => {
        cca.toast.show('error_msg_save_file_failed');
        throw error;
      });
    }
  }).catch(console.error).finally(() => {
    // Re-enable UI controls after finishing the take.
    this.take_ = null;
    cca.state.set('taking', false);
    this.updateShutterLabel_();
  });
};

/**
 * Starts a recording to create a blob of it after the recorder is stopped.
 * @return {!Promise<Blob>} Promise for the result.
 * @private
 */
cca.views.Camera.prototype.createRecordingBlob_ = function() {
  return new Promise((resolve, reject) => {
    var recordedChunks = [];
    var ondataavailable = (event) => {
      // TODO(yuli): Handle insufficient storage.
      if (event.data && event.data.size > 0) {
        recordedChunks.push(event.data);
      }
    };
    var onstop = (event) => {
      this.mediaRecorder_.removeEventListener('dataavailable', ondataavailable);
      this.mediaRecorder_.removeEventListener('stop', onstop);
      this.recordTime_.stop();

      var recordedBlob = new Blob(
          recordedChunks, {type: cca.views.Camera.RECORD_MIMETYPE});
      recordedChunks = [];
      if (recordedBlob.size) {
        resolve(recordedBlob);
      } else {
        reject(new Error('Recording blob error.'));
      }
    };
    this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
    this.mediaRecorder_.addEventListener('stop', onstop);

    // Start recording and update the UI for the ongoing recording.
    // TODO(yuli): Don't re-enable audio after crbug.com/878255 fixed in M73.
    var track = this.preview_.stream.getAudioTracks()[0];
    var enableAudio = (enabled) => {
      if (track) {
        track.enabled = enabled;
      }
    };
    enableAudio(true);
    this.mediaRecorder_.start();
    enableAudio(cca.state.get('mic'));
    this.recordTime_.start();
  });
};

/**
 * Takes a photo to create a blob of it.
 * @return {!Promise<Blob>} Promise for the result.
 * @private
 */
cca.views.Camera.prototype.createPhotoBlob_ = function() {
  // Enable using image-capture to take photo only on ChromeOS after M68.
  // TODO(yuli): Remove this restriction if no longer applicable.
  if (cca.util.isChromeOS() && cca.util.isChromeVersionAbove(68)) {
    var getPhotoCapabilities = () => {
      if (this.photoCapabilities_ == null) {
        this.photoCapabilities_ = this.imageCapture_.getPhotoCapabilities();
      }
      return this.photoCapabilities_;
    };
    return getPhotoCapabilities().then((photoCapabilities) => {
      // Set to take the highest resolution, but the photo to be taken will
      // still have the same aspect ratio with the preview.
      var photoSettings = {
        imageWidth: photoCapabilities.imageWidth.max,
        imageHeight: photoCapabilities.imageHeight.max,
      };
      return this.imageCapture_.takePhoto(photoSettings);
    });
  } else {
    return this.preview_.toImage();
  }
};

/**
 * Prepares the media-recorder for the current stream.
 * @private
 */
cca.views.Camera.prototype.prepareMediaRecorder_ = function() {
  if (this.mediaRecorder_ == null) {
    if (!MediaRecorder.isTypeSupported(cca.views.Camera.RECORD_MIMETYPE)) {
      throw new Error('The preferred mimeType is not supported.');
    }
    this.mediaRecorder_ = new MediaRecorder(
        this.preview_.stream, {mimeType: cca.views.Camera.RECORD_MIMETYPE});
  }
};

/**
 * Prepares the image-capture for the current stream.
 * @private
 */
cca.views.Camera.prototype.prepareImageCapture_ = function() {
  if (this.imageCapture_ == null) {
    this.imageCapture_ = new ImageCapture(
        this.preview_.stream.getVideoTracks()[0]);
  }
};

/**
 * Returns constraints-candidates for all available video-devices.
 * @return {!Promise<Array<Object>>} Promise for the result.
 * @private
 */
cca.views.Camera.prototype.constraintsCandidates_ = function() {
  var deviceConstraints = (deviceId, recordMode) => {
    // Constraints are ordered by priority.
    return [
      {
        aspectRatio: {ideal: recordMode ? 1.7777777778 : 1.3333333333},
        width: {min: 1280},
        frameRate: {min: 24},
      },
      {
        width: {min: 640},
        frameRate: {min: 24},
      },
    ].map((constraint) => {
      // Each passed-in video-constraint will be modified here.
      if (deviceId) {
        constraint.deviceId = {exact: deviceId};
      } else {
        // As a default camera use the one which is facing the user.
        constraint.facingMode = {exact: 'user'};
      }
      return {audio: recordMode, video: constraint};
    });
  };

  return this.options_.videoDeviceIds().then((deviceIds) => {
    var recordMode = this.recordMode;
    var candidates = [];
    deviceIds.forEach((deviceId) => {
      candidates = candidates.concat(deviceConstraints(deviceId, recordMode));
    });
    return candidates;
  });
};

/**
 * Stops camera and tries to start camera stream again if possible.
 * @return {!Promise} Promise for the start-camera operation.
 * @private
 */
cca.views.Camera.prototype.stop_ = function() {
  // Update shutter label as record-mode might be toggled before reaching here.
  this.updateShutterLabel_();
  // Wait for ongoing 'start' and 'take' done before restarting camera.
  return Promise.all([
    this.started_,
    Promise.resolve(!cca.state.get('taking') || this.endTake_()),
  ]).finally(() => {
    this.preview_.stop();
    this.mediaRecorder_ = null;
    this.imageCapture_ = null;
    this.photoCapabilities_ = null;
    this.start_();
    return this.started_;
  });
};

/**
 * Starts camera if the camera stream was stopped.
 * @private
 */
cca.views.Camera.prototype.start_ = function() {
  var suspend = this.locked_ || chrome.app.window.current().isMinimized();
  this.started_ = (suspend ? Promise.reject(new Error('suspend')) :
      this.constraintsCandidates_()).then((candidates) => {
    var tryStartWithCandidate = (index) => {
      if (index >= candidates.length) {
        return Promise.reject(new Error('out-of-candidates'));
      }
      var constraints = candidates[index];
      return navigator.mediaDevices.getUserMedia(constraints).then(
          this.preview_.start.bind(this.preview_)).then(() => {
        this.options_.updateValues(constraints, this.preview_.stream);
        cca.nav.close('warning', 'no-camera');
      }).catch((error) => {
        console.error(error);
        return new Promise((resolve) => {
          // TODO(mtomasz): Workaround for crbug.com/383241.
          setTimeout(() => resolve(tryStartWithCandidate(index + 1)), 0);
        });
      });
    };
    return tryStartWithCandidate(0);
  }).catch((error) => {
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
