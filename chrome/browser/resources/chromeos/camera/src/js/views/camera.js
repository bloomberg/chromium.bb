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
   * Promise for play sound delay.
   * @type {?Promise}
   * @private
   */
  this.deferred_capture_ = null;

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
  this.take_ =
      cca.views.camera.timertick.start()
          .then(() => {
            // Play a sound before starting to record and delay the take to
            // avoid the sound being recorded if necessary.
            this.deferred_capture_ =
                this.recordMode ? cca.sound.play('#sound-rec-start') : null;
            return this.deferred_capture_ &&
                this.deferred_capture_.finally(
                    () => this.deferred_capture_ = null);
          })
          .then(() => {
            if (this.recordMode) {
              try {
                this.prepareMediaRecorder_();
              } catch (e) {
                cca.toast.show('error_msg_record_start_failed');
                throw e;
              }
              // Take of recording will be ended by another shutter click.
              return this.createRecordingBlob_().catch((e) => {
                cca.toast.show('error_msg_empty_recording');
                throw e;
              });
            } else {
              try {
                this.prepareImageCapture_();
              } catch (e) {
                cca.toast.show('error_msg_take_photo_failed');
                throw e;
              }
              return this.createPhotoBlob_().catch((e) => {
                cca.toast.show('error_msg_take_photo_failed');
                throw e;
              });
            }
          })
          .then((blob) => {
            if (blob) {
              // Play a sound and save the result after a successful take.
              cca.metrics.log(
                  cca.metrics.Type.CAPTURE, this.facingMode_, blob.mins);
              var recordMode = this.recordMode;
              cca.sound.play(recordMode ? '#sound-rec-end' : '#sound-shutter');
              return this.model_.savePicture(blob, recordMode)
                  .catch((e) => {
                    cca.toast.show('error_msg_save_file_failed');
                    throw e;
                  });
            }
          })
          .catch((e) => {
            if (e && e.message == 'cancel') {
              return;
            }
            console.error(e);
          })
          .finally(() => {
            this.take_ = null;
            cca.state.set('taking', false);
            this.focus();  // Refocus the visible shutter button for ChromeVox.
          });
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
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.Camera.prototype.endTake_ = function() {
  cca.views.camera.timertick.cancel();
  if (this.deferred_capture_ && this.deferred_capture_.cancel) {
    this.deferred_capture_.cancel();
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }

  return Promise.resolve(this.take_);
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

      var recordedBlob = new Blob(
          recordedChunks, {type: cca.views.Camera.RECORD_MIMETYPE});
      recordedBlob.mins = this.recordTime_.stop();
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
        this.facingMode_ = this.options_.updateValues(
            constraints, this.preview_.stream);
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
