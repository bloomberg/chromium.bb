// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
 * Creates the camera-view controller.
 * @param {camera.Router} router View router to switch views.
 * @param {camera.models.Gallery} model Model object.
 * @param {function(number)} onAspectRatio Callback to report aspect ratio.
 * @constructor
 */
camera.views.Camera = function(router, model, onAspectRatio) {
  camera.View.call(this, router, document.querySelector('#camera'), 'camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {camera.models.Gallery}
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
   * @type {camera.views.camera.Layout}
   * @private
   */
  this.layout_ = new camera.views.camera.Layout();

  /**
   * Video preview for the camera.
   * @type {camera.views.camera.Preview}
   * @private
   */
  this.preview_ = new camera.views.camera.Preview(
      this.stop_.bind(this), onAspectRatio);

  /**
   * Options for the camera.
   * @type {camera.views.camera.Options}
   * @private
   */
  this.options_ = new camera.views.camera.Options(
      router, this.stop_.bind(this));

  /**
   * Record-time for the elapsed recording time.
   * @type {camera.views.camera.RecordTime}
   * @private
   */
  this.recordTime_ = new camera.views.camera.RecordTime();

  /**
   * Button for going to the gallery.
   * @type {camera.views.camera.GalleryButton}
   * @private
   */
  this.galleryButton_ = new camera.views.camera.GalleryButton(router, model);

  /**
   * Button for taking photos and recording videos.
   * @type {HTMLButtonElement}
   * @private
   */
  this.shutterButton_ = document.querySelector('#shutter');

  /**
   * @type {string}
   * @private
   */
  this.keyBuffer_ = '';

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
   * @type {Promise<>}
   * @private
   */
  this.started_ = null;

  /**
   * Promise for the current timer ticks.
   * @type {Promise<>}
   * @private
   */
  this.ticks_ = null;

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
      this.onShutterButtonClicked_.bind(this));
};

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred format.
 * @type {string}
 * @const
 */
camera.views.Camera.RECORD_MIMETYPE = 'video/x-matroska;codecs=avc1';

camera.views.Camera.prototype = {
  __proto__: camera.View.prototype,
  get capturing() {
    return document.body.classList.contains('capturing');
  },
  get taking() {
    return document.body.classList.contains('taking');
  },
  get recordMode() {
    return document.body.classList.contains('record-mode');
  },
  get galleryButton() {
    return this.galleryButton_;
  },
};

/**
 * Prepares the view.
 */
camera.views.Camera.prototype.prepare = function() {
  // Monitor the states to stop camera when locked/minimized.
  chrome.idle.onStateChanged.addListener(newState => {
    this.locked_ = (newState == 'locked');
    if (this.locked_) {
      this.stop_();
    }
  });
  chrome.app.window.current().onMinimized.addListener(() => {
    this.stop_();
  });
  // Start the camera after preparing the options (device ids).
  this.options_.prepare();
  this.start_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onEnter = function() {
  this.onResize();
};

/**
 * @override
 */
camera.views.Camera.prototype.onActivate = function() {
  if (document.activeElement != document.body) {
    this.shutterButton_.focus();
  }
};

/**
 * Handles clicking on the shutter button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onShutterButtonClicked_ = function(event) {
  if (this.taking) {
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
    camera.toast.show(this.recordMode ?
        'errorMsgRecordStartFailed' : 'errorMsgTakePhotoFailed');
  }
};

/**
 * Updates UI controls for capturing/taking state changes.
 * @private
 */
camera.views.Camera.prototype.updateControls_ = function() {
  // Update the shutter's label before enabling or disabling it.
  var [capturing, taking] = [this.capturing, this.taking];
  this.updateShutterLabel_();
  this.shutterButton_.disabled = !capturing;
  this.options_.updateControls(capturing, taking);
  this.galleryButton_.disabled = !capturing || taking;
};

/**
 * Updates the shutter button's label.
 * @private
 */
camera.views.Camera.prototype.updateShutterLabel_ = function() {
  var label;
  if (this.recordMode) {
    label = this.taking ? 'recordVideoStopButton' : 'recordVideoStartButton';
  } else {
    label = (this.taking && this.ticks_) ?
        'takePhotoCancelButton' : 'takePhotoButton';
  }
  this.shutterButton_.setAttribute('i18n-label', label);
  this.shutterButton_.setAttribute('aria-label', chrome.i18n.getMessage(label));
};

/**
 * @override
 */
camera.views.Camera.prototype.onResize = function() {
  this.layout_.update();
};

/**
 * @override
 */
camera.views.Camera.prototype.onKeyPressed = function(event) {
  this.keyBuffer_ += String.fromCharCode(event.which);
  this.keyBuffer_ = this.keyBuffer_.substr(-10);

  if (this.keyBuffer_.indexOf('VER') !== -1) {
    camera.toast.show(chrome.runtime.getManifest().version);
    this.keyBuffer_ = '';
  }
  if (this.keyBuffer_.indexOf('RES') !== -1) {
    camera.toast.show(this.preview_.toString());
    this.keyBuffer_ = '';
  }
};

/**
 * Begins to take photo or recording with the current options, e.g. timer.
 * @private
 */
camera.views.Camera.prototype.beginTake_ = function() {
  document.body.classList.add('taking');
  this.ticks_ = this.options_.timerTicks();
  this.updateControls_();

  Promise.resolve(this.ticks_).then(() => {
    // Play a sound before starting to record and delay the take to avoid the
    // sound being recorded if necessary.
    var delay = (this.recordMode && this.options_.playSound(
        camera.views.camera.Options.Sound.RECORDSTART)) ? 250 : 0;
    this.takeTimeout_ = setTimeout(() => {
      if (this.recordMode) {
        // Take of recording will be ended by another shutter click.
        this.take_ = this.createRecordingBlob_().catch(error => {
          throw [error, 'errorMsgEmptyRecording'];
        });
      } else {
        this.take_ = this.createPhotoBlob_().catch(error => {
          throw [error, 'errorMsgTakePhotoFailed'];
        });
        this.endTake_();
      }
    }, delay);
  }).catch(() => {});
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise<>} Promise for the operation.
 * @private
 */
camera.views.Camera.prototype.endTake_ = function() {
  if (this.ticks_) {
    this.ticks_.cancel();
    this.ticks_ = null;
  }
  if (this.takeTimeout_) {
    clearTimeout(this.takeTimeout_);
    this.takeTimeout_ = null;
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }

  return Promise.resolve(this.take_).then(blob => {
    if (blob && !blob.handled) {
      // Play a sound and save the result after a successful take.
      blob.handled = true;
      var recordMode = this.recordMode;
      this.options_.playSound(recordMode ?
          camera.views.camera.Options.Sound.RECORDEND :
          camera.views.camera.Options.Sound.SHUTTER);
      return this.model_.savePicture(blob, recordMode).catch(error => {
        throw [error, 'errorMsgSaveFileFailed'];
      });
    }
  }).catch(([error, message]) => {
    console.error(error);
    camera.toast.show(message);
  }).finally(() => {
    // Re-enable UI controls after finishing the take.
    this.take_ = null;
    document.body.classList.remove('taking');
    this.updateControls_();
  });
};

/**
 * Starts a recording to create a blob of it after the recorder is stopped.
 * @return {!Promise<Blob>} Promise for the result.
 * @private
 */
camera.views.Camera.prototype.createRecordingBlob_ = function() {
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
          recordedChunks, {type: camera.views.Camera.RECORD_MIMETYPE});
      recordedChunks = [];
      if (recordedBlob.size) {
        resolve(recordedBlob);
      } else {
        reject('Recording blob error.');
      }
    };
    this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
    this.mediaRecorder_.addEventListener('stop', onstop);

    // Start recording and update the UI for the ongoing recording. Force to
    // re-enable audio track before starting recording (crbug.com/878255).
    this.options_.updateMicAudio(true);
    this.mediaRecorder_.start();
    this.options_.updateMicAudio();
    this.recordTime_.start();
  });
};

/**
 * Takes a photo to create a blob of it.
 * @return {!Promise<Blob>} Promise for the result.
 * @private
 */
camera.views.Camera.prototype.createPhotoBlob_ = function() {
  // Enable using image-capture to take photo only on ChromeOS after M68.
  // TODO(yuli): Remove this restriction if no longer applicable.
  if (camera.util.isChromeOS() && camera.util.isChromeVersionAbove(68)) {
    var getPhotoCapabilities = () => {
      if (this.photoCapabilities_ == null) {
        this.photoCapabilities_ = this.imageCapture_.getPhotoCapabilities();
      }
      return this.photoCapabilities_;
    };
    return getPhotoCapabilities().then(photoCapabilities => {
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
camera.views.Camera.prototype.prepareMediaRecorder_ = function() {
  if (this.mediaRecorder_ == null) {
    if (!MediaRecorder.isTypeSupported(camera.views.Camera.RECORD_MIMETYPE)) {
      throw 'The preferred mimeType is not supported.';
    }
    this.mediaRecorder_ =  new MediaRecorder(
        this.preview_.stream, {mimeType: camera.views.Camera.RECORD_MIMETYPE});
  }
};

/**
 * Prepares the image-capture for the current stream.
 * @private
 */
camera.views.Camera.prototype.prepareImageCapture_ = function() {
  if (this.imageCapture_ == null) {
    this.imageCapture_ = new ImageCapture(
        this.preview_.stream.getVideoTracks()[0]);
  }
};

/**
 * Returns constraints-candidates with the specified device id.
 * @param {string} deviceId Device id to be set in the constraints.
 * @return {Array<Object>}
 * @private
 */
camera.views.Camera.prototype.constraintsCandidates_ = function(deviceId) {
  var recordMode = this.recordMode;
  var videoConstraints =
      [{
        aspectRatio: { ideal: recordMode ? 1.7777777778 : 1.3333333333 },
        width: { min: 1280 },
        frameRate: { min: 24 },
      },
      {
        width: { min: 640 },
        frameRate: { min: 24 },
      }];

  // Constraints are ordered by priority.
  return videoConstraints.map((constraint) => {
    // Each passed-in video-constraint will be modified here.
    if (deviceId) {
      constraint.deviceId = { exact: deviceId };
    } else {
      // As a default camera use the one which is facing the user.
      constraint.facingMode = { exact: 'user' };
    }
    return { audio: recordMode, video: constraint };
  });
};

/**
 * Stops camera and tries to start camera stream again if possible.
 * @return {!Promise<>} Promise for the start-camera operation.
 * @private
 */
camera.views.Camera.prototype.stop_ = function() {
  // Wait for ongoing 'start' and 'take' done before restarting camera.
  return Promise.all([
    this.started_,
    Promise.resolve(!this.taking || this.endTake_())
  ]).finally(() => {
    this.preview_.stop();
    this.mediaRecorder_ = null;
    this.imageCapture_ = null;
    this.photoCapabilities_ = null;
    document.body.classList.remove('capturing');
    this.updateControls_();
    this.start_();
    return this.started_;
  });
};

/**
 * Starts camera if the camera stream was stopped.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  // TODO(yuli): Check 'suspend' before getting device-ids.
  this.started_ = this.options_.videoDeviceIds().then((deviceIds) => {
    var candidates = [];
    deviceIds.forEach((deviceId) => {
      candidates = candidates.concat(this.constraintsCandidates_(deviceId));
    });

    var tryStartWithConstraints = (index) => {
      if (this.locked_ || chrome.app.window.current().isMinimized()) {
        return Promise.reject('suspend');
      }
      if (index >= candidates.length) {
        return Promise.reject('out-of-candidates');
      }

      var constraints = candidates[index];
      return navigator.mediaDevices.getUserMedia(constraints).then(
          this.preview_.start.bind(this.preview_)).then(() => {
        this.options_.updateValues(constraints, this.preview_.stream);
        document.body.classList.add('capturing');
        this.updateControls_();
        camera.App.onErrorRecovered('no-camera');
      }).catch((error) => {
        console.error(error);
        return new Promise((resolve) => {
          // TODO(mtomasz): Workaround for crbug.com/383241.
          setTimeout(() => resolve(tryStartWithConstraints(index + 1)), 0);
        });
      });
    };
    return tryStartWithConstraints(0);
  }).catch((error) => {
    if (error != 'suspend') {
      console.error(error);
      camera.App.onError('no-camera', 'errorMsgNoCamera');
    }
    // Schedule to retry.
    if (this.retryStartTimeout_) {
      clearTimeout(this.retryStartTimeout_);
      this.retryStartTimeout_ = null;
    }
    this.retryStartTimeout_ = setTimeout(this.start_.bind(this), 100);
  });
};
