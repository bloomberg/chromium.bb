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
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @param {camera.models.Gallery} model Model object.
 * @constructor
 */
camera.views.Camera = function(context, router, model) {
  camera.View.call(
      this, context, router, document.querySelector('#camera'), 'camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * Video element to capture the stream.
   * @type {Video}
   * @private
   */
  this.video_ = document.querySelector('#main-preview');

  /**
   * Current camera stream.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

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
   * @type {?number}
   * @private
   */
  this.retryStartTimeout_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.locked_ = false;

  /**
   * Toast for showing the messages.
   * @type {camera.views.Toast}
   * @private
   */
  this.toast_ = new camera.views.Toast();

  /**
   * Options for the camera-view.
   * @type {camera.views.camera.Options}
   * @private
   */
  this.options_ = new camera.views.camera.Options(
      router, this.stop_.bind(this));

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
  this.shutterButton_ = document.querySelector('#take-picture');

  /**
   * Timer for elapsed recording time for video recording.
   * @type {?number}
   * @private
   */
  this.recordingTimer_ = null;

  /**
   * @type {string}
   * @private
   */
  this.keyBuffer_ = '';

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

  this.shutterButton_.addEventListener(
      'click', this.onShutterButtonClicked_.bind(this));
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
  get galleryButton() {
    return this.galleryButton_;
  }
};

/**
 * Prepares the view.
 */
camera.views.Camera.prototype.prepare = function() {
  // Monitor the locked state to avoid retrying camera connection when locked.
  chrome.idle.onStateChanged.addListener(newState => {
    this.locked_ = (newState == 'locked');
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
  if (this.options_.recordMode) {
    if (this.mediaRecorder_ == null) {
      // Create a media-recorder before proceeding to record video.
      this.mediaRecorder_ = this.createMediaRecorder_(this.stream_);
      if (this.mediaRecorder_ == null) {
        this.showToastMessage_('errorMsgRecordStartFailed', true);
        return;
      }
    }
  } else {
    if (this.imageCapture_ == null) {
      // Create a image-capture before proceeding to take photo.
      var track = this.stream_ && this.stream_.getVideoTracks()[0];
      this.imageCapture_ = track && new ImageCapture(track);
      if (this.imageCapture_ == null) {
        this.showToastMessage_('errorMsgTakePhotoFailed', true);
        return;
      }
    }
  }
  if (this.taking) {
    // End the prior ongoing take (recording); a new take shouldn't be started
    // until the prior one is ended.
    this.endTake_();
  } else {
    this.beginTake_();
  }
};

/**
 * Updates UI controls' disabled status for capturing/taking state changes.
 * @private
 */
camera.views.Camera.prototype.updateControls_ = function() {
  var disabled = this.taking || !this.capturing;
  this.options_.disabled = disabled;
  this.shutterButton_.disabled = disabled;
  this.galleryButton_.disabled = disabled;
};

/**
 * Sets the shutter button's i18n and aria label.
 * @param {string} label Label to be set.
 * @private
 */
camera.views.Camera.prototype.setShutterLabel_ = function(label) {
  this.shutterButton_.setAttribute('i18n-label', label);
  this.shutterButton_.setAttribute('aria-label', chrome.i18n.getMessage(label));
};

/**
 * @override
 */
camera.views.Camera.prototype.onResize = function() {
  this.updateVideoSize_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onKeyPressed = function(event) {
  this.keyBuffer_ += String.fromCharCode(event.which);
  this.keyBuffer_ = this.keyBuffer_.substr(-10);

  if (this.keyBuffer_.indexOf('VER') !== -1) {
    this.showToastMessage_(chrome.runtime.getManifest().version, false);
    this.keyBuffer_ = '';
  }
  if (this.keyBuffer_.indexOf('RES') !== -1) {
    if (this.video_.videoWidth || this.video_.videoHeight) {
      this.showToastMessage_(
          this.video_.videoWidth + ' x ' + this.video_.videoHeight, false);
    }
    this.keyBuffer_ = '';
  }
};

/**
 * Shows a non-intrusive toast message.
 * @param {string} message Message to be shown.
 * @param {boolean} named True if it's i18n named message, false otherwise.
 * @private
 */
camera.views.Camera.prototype.showToastMessage_ = function(message, named) {
  this.toast_.showMessage(named ? chrome.i18n.getMessage(message) : message);
};

/**
 * Starts to show the timer of elapsed recording time.
 * TODO(yuli): Extract recording-timer as RecordTime.show/hide.
 * @private
 */
camera.views.Camera.prototype.showRecordingTimer_ = function() {
  // Format number of seconds into "HH:MM:SS" string.
  var formatSeconds = function(seconds) {
    var sec = seconds % 60;
    var min = Math.floor(seconds / 60) % 60;
    var hr = Math.floor(seconds / 3600);
    if (sec < 10) {
      sec = '0' + sec;
    }
    if (min < 10) {
      min = '0' + min;
    }
    if (hr < 10) {
      hr = '0' + hr;
    }
    return hr + ':' + min + ':' + sec;
  };

  var timerElement = document.querySelector('#recording-timer');
  timerElement.classList.add('visible');
  var timerTicks = 0;
  timerElement.textContent = formatSeconds(timerTicks);

  this.recordingTimer_ = setInterval(function() {
    timerTicks++;
    timerElement.textContent = formatSeconds(timerTicks);
  }, 1000);
};

/**
 * Stops and hides the timer of recording time.
 * @private
 */
camera.views.Camera.prototype.hideRecordingTimer_ = function() {
  if (this.recordingTimer_) {
    clearInterval(this.recordingTimer_);
    this.recordingTimer_ = null;
  }

  var timerElement = document.querySelector('#recording-timer');
  timerElement.textContent = '';
  timerElement.classList.remove('visible');
};

/**
 * Begins to take photo or recording with the current options, e.g. timer.
 * @private
 */
camera.views.Camera.prototype.beginTake_ = function() {
  document.body.classList.add('taking');
  this.updateControls_();

  this.options_.onTimerTicks(recordMode => {
    // Play a sound before starting to record and delay the take to avoid the
    // sound being recorded if necessary.
    var delay = (recordMode && this.options_.onSound(
        camera.views.camera.Options.Sound.RECORDSTART)) ? 250 : 0;
    this.takeTimeout_ = setTimeout(() => {
      if (recordMode) {
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
  });
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @private
 */
camera.views.Camera.prototype.endTake_ = function() {
  this.options_.cancelTimerTicks();
  if (this.takeTimeout_) {
    clearTimeout(this.takeTimeout_);
    this.takeTimeout_ = null;
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }

  // Re-enable UI controls after finishing the current take.
  Promise.resolve(this.take_ || null).then(blob => {
    if (blob == null) {
      // There is no ongoing take.
      return;
    }
    // Play a sound and save the result after a successful take.
    var recordMode = this.options_.recordMode;
    this.options_.onSound(recordMode ?
        camera.views.camera.Options.Sound.RECORDEND :
        camera.views.camera.Options.Sound.SHUTTER);
    return this.model_.savePicture(blob, recordMode).catch(error => {
      throw [error, 'errorMsgSaveFileFailed'];
    });
  }).catch(([error, toast]) => {
    console.error(error);
    this.showToastMessage_(toast, true);
  }).finally(() => {
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
      this.hideRecordingTimer_();
      this.shutterButton_.classList.remove('flash');
      this.setShutterLabel_('recordVideoStartButton');

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

    // Start recording and update the UI for the ongoing recording.
    this.mediaRecorder_.start();
    this.shutterButton_.classList.add('flash');
    this.showRecordingTimer_();

    // Re-enable the shutter button to stop recording later and flash the
    // shutter button until the recording is stopped.
    this.setShutterLabel_('recordVideoStopButton');
    this.shutterButton_.disabled = false;
  });
};

/**
 * Takes a photo to create a blob of it.
 * @return {!Promise<Blob>} Promise for the result.
 * @private
 */
camera.views.Camera.prototype.createPhotoBlob_ = function() {
  // Since the takePhoto() API has not been implemented on CrOS until 68, use
  // canvas-drawing as a fallback for older Chrome versions.
  // TODO(shenghao): Remove canvas-drawing after min-chrome-version is above 68.
  if (camera.util.isChromeVersionAbove(68)) {
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
        imageHeight: photoCapabilities.imageHeight.max
      };
      return this.imageCapture_.takePhoto(photoSettings);
    });
  } else {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    canvas.width = this.video_.videoWidth;
    canvas.height = this.video_.videoHeight;
    context.drawImage(this.video_, 0, 0);
    return new Promise((resolve, reject) => {
      canvas.toBlob(blob => {
        if (blob) {
          resolve(blob);
        } else {
          reject('Photo blob error.');
        }
      }, 'image/jpeg');
    });
  }
};

/**
 * Creates the media recorder for the video stream.
 * @param {MediaStream} stream Stream to be recorded.
 * @return {MediaRecorder} Media recorder created.
 * @private
 */
camera.views.Camera.prototype.createMediaRecorder_ = function(stream) {
  try {
    if (!MediaRecorder.isTypeSupported(camera.views.Camera.RECORD_MIMETYPE)) {
      throw 'The preferred mimeType is not supported.';
    }
    var options = {mimeType: camera.views.Camera.RECORD_MIMETYPE};
    return new MediaRecorder(stream, options);
  } catch (e) {
    console.error('Unable to create MediaRecorder: ' + e);
    return null;
  }
};

/**
 * Starts capturing with the specified constraints.
 * @param {!Object} constraints Constraints passed to WebRTC.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback, eg. the constraints are
 *     not supported.
 * @private
 */
camera.views.Camera.prototype.startWithConstraints_ = function(
    constraints, onSuccess, onFailure) {
  navigator.mediaDevices.getUserMedia(constraints).then(stream => {
    // Mute to avoid echo from the captured audio.
    this.video_.muted = true;
    this.video_.srcObject = stream;
    var onLoadedMetadata = () => {
      this.video_.removeEventListener('loadedmetadata', onLoadedMetadata);
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog_ = setInterval(() => {
        // Check if video stream is ended (audio stream may still be live).
        if (!stream.getVideoTracks().length ||
            stream.getVideoTracks()[0].readyState == 'ended') {
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
          this.onstop_();
        }
      }, 100);
      this.context_.onAspectRatio(
          this.video_.videoWidth / this.video_.videoHeight);
      this.updateVideoSize_();

      this.stream_ = stream;
      this.options_.updateStreamOptions(constraints, stream);
      this.setShutterLabel_(this.options_.recordMode ?
          'recordVideoStartButton' : 'takePhotoButton');
      document.body.classList.add('capturing');
      this.updateControls_();
      this.shutterButton_.focus();
      onSuccess();
    };
    // Load the stream and wait for the metadata.
    this.video_.addEventListener('loadedmetadata', onLoadedMetadata);
    this.video_.play();
  }, onFailure);
};

/**
 * Updates the video element's size for previewing in the window.
 * @private
 */
camera.views.Camera.prototype.updateVideoSize_ = function() {
  if (!this.video_.videoHeight) {
    return;
  }
  // The video content keeps its aspect ratio and is filled up or letterboxed
  // inside the window's inner-bounds. Don't use app-window.innerBounds' width
  // and height properties during resizing as they are not updated immediately.
  var fill = !camera.util.isWindowFullSize();
  var f = fill ? Math.max : Math.min;
  var scale = f(window.innerHeight / this.video_.videoHeight,
      window.innerWidth / this.video_.videoWidth);
  this.video_.width = scale * this.video_.videoWidth;
  this.video_.height = scale * this.video_.videoHeight;
  // TODO(yuli); Align letterboxed video-element by new UI spec.
}

/**
 * Stop handler when the camera stream is stopped.
 * @private
 */
camera.views.Camera.prototype.onstop_ = function() {
  if (this.taking) {
    this.endTake_();
  }
  this.mediaRecorder_ = null;
  this.imageCapture_ = null;
  this.photoCapabilities_ = null;
  this.stream_ = null;
  document.body.classList.remove('capturing');
  this.updateControls_();
  // Try reconnecting the camera to capture new streams.
  this.start_();
};

/**
 * Stops the camera stream so it retries opening the camera stream on new
 * device or with new constraints.
 * @private
 */
camera.views.Camera.prototype.stop_ = function() {
  if (this.watchdog_) {
    clearInterval(this.watchdog_);
    this.watchdog_ = null;
  }
  // TODO(mtomasz): Prevent blink. Clear somehow the video tag.
  if (this.stream_) {
    this.stream_.getVideoTracks()[0].stop();
  }
  this.onstop_();
};

/**
 * Returns constraints-candidates with the specified device id.
 * @param {string} deviceId Device id to be set in the constraints.
 * @return {Array<Object>}
 * @private
 */
camera.views.Camera.prototype.constraintsCandidates_ = function(deviceId) {
  var recordMode = this.options_.recordMode;
  var videoConstraints = () => {
    if (recordMode) {
      // Video constraints for video recording are ordered by priority.
      return [
          {
            aspectRatio: { ideal: 1.7777777778 },
            width: { min: 1280 },
            frameRate: { min: 24 }
          },
          {
            width: { min: 640 },
            frameRate: { min: 24 }
          }];
    } else {
      // Video constraints for photo taking are ordered by priority.
      return [
          {
            aspectRatio: { ideal: 1.3333333333 },
            width: { min: 1280 },
            frameRate: { min: 24 }
          },
          {
            width: { min: 640 },
            frameRate: { min: 24 }
          }];
    }
  };

  return videoConstraints().map(videoConstraint => {
    // Each passed-in video-constraint will be modified here.
    if (deviceId) {
      videoConstraint.deviceId = { exact: deviceId };
    } else {
      // As a default camera use the one which is facing the user.
      videoConstraint.facingMode = { exact: 'user' };
    }
    return { audio: recordMode, video: videoConstraint };
  });
};

/**
 * Starts capturing the camera with the highest possible resolution.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var scheduleRetry = () => {
    if (this.retryStartTimeout_) {
      clearTimeout(this.retryStartTimeout_);
      this.retryStartTimeout_ = null;
    }
    this.retryStartTimeout_ = setTimeout(this.start_.bind(this), 100);
  };

  var onFailure = (error) => {
    console.error(error);
    this.context_.onError('no-camera',
        chrome.i18n.getMessage('errorMsgNoCamera'),
        chrome.i18n.getMessage('errorMsgNoCameraHint'));
    scheduleRetry();
  };

  var constraintsCandidates = [];

  var tryStartWithConstraints = (index) => {
    if (this.locked_) {
      scheduleRetry();
      return;
    }
    if (index >= constraintsCandidates.length) {
      onFailure();
      return;
    }
    this.startWithConstraints_(constraintsCandidates[index], results => {
      if (this.retryStartTimeout_) {
        clearTimeout(this.retryStartTimeout_);
        this.retryStartTimeout_ = null;
      }
      // Remove the error layer if any.
      this.context_.onErrorRecovered('no-camera');
    }, error => {
      if (error && error.name != 'ConstraintNotSatisfiedError') {
        // Constraint errors are expected, so don't report them.
        console.error(error);
      }
      // TODO(mtomasz): Workaround for crbug.com/383241.
      setTimeout(tryStartWithConstraints.bind(this, index + 1), 0);
    });
  };

  this.options_.videoDeviceIds().then(deviceIds => {
    deviceIds.forEach(deviceId => {
      constraintsCandidates = constraintsCandidates.concat(
          this.constraintsCandidates_(deviceId));
    });
    tryStartWithConstraints(0);
  }).catch(onFailure);
};
