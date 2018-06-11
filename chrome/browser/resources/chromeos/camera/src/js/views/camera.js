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
 * Creates the Camera view controller.
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.Camera = function(context, router) {
  camera.View.call(
      this, context, router, document.querySelector('#camera'), 'camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = null;

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
  this.retryStartTimer_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Shutter sound player.
   * @type {Audio}
   * @private
   */
  this.shutterSound_ = document.createElement('audio');

  /**
   * Tick sound player.
   * @type {Audio}
   * @private
   */
  this.tickSound_ = document.createElement('audio');

  /**
   * Record-start sound player.
   * @type {Audio}
   * @private
   */
  this.recordStartSound_ = document.createElement('audio');

  /**
   * Record-end sound player.
   * @type {Audio}
   * @private
   */
  this.recordEndSound_ = document.createElement('audio');

  /**
   * @type {boolean}
   * @private
   */
  this.locked_ = false;

  /**
   * Timer for hiding the toast message after some delay.
   * @type {number?}
   * @private
   */
  this.toastHideTimer_ = null;

  /**
   * Toast transition wrapper. Shows or hides the toast with the passed message.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toastEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toastElement = document.querySelector('#toast');
        var toastMessageElement = document.querySelector('#toast-message');
        // Hide the message if visible.
        if (!args.visible && toastElement.classList.contains('visible')) {
          toastElement.classList.remove('visible');
          camera.util.waitForTransitionCompletion(
              toastElement, 500, callback);
        } else if (args.visible) {
          // If showing requested, then show.
          toastMessageElement.textContent = args.message;
          toastElement.classList.add('visible');
          camera.util.waitForTransitionCompletion(
             toastElement, 500, callback);
        } else {
          callback();
        }
      }.bind(this));

  /**
   * Timer for elapsed recording time for video recording.
   * @type {number?}
   * @private
   */
  this.recordingTimer_ = null;

  /**
   * Whether a picture is being taken. Used for updating toolbar UI.
   * @type {boolean}
   * @private
   */
  this.taking_ = false;

  /**
   * Whether the camera is in video recording mode.
   * @type {boolean}
   * @private
   */
  this.is_recording_mode_ = false;

  /**
   * @type {string}
   * @private
   */
  this.keyBuffer_ = '';

  /**
   * Timer used to countdown before taking the picture.
   * @type {number?}
   * @private
   */
  this.tickTakePictureTimer_ = null;

  /**
   * Timer for do-take-picture calls.
   * @type {number?}
   * @private
   */
  this.doTakePictureTimer_ = null;

  /**
   * DeviceId of the camera device used for the last time during this session.
   * @type {string?}
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
   * Legacy mirroring set per all devices.
   * @type {boolean}
   * @private
   */
  this.legacyMirroringToggle_ = true;

  /**
   * Mirroring set per device.
   * @type {!Object}
   * @private
   */
  this.mirroringToggles_ = {};

  // End of properties, seal the object.
  Object.seal(this);

  // TODO(yuli): Disable resizing window and simplify code here.
  this.video_.addEventListener('loadedmetadata',
      this.updateVideoSize_.bind(this));
  this.video_.addEventListener('resize',
      this.updateVideoSize_.bind(this));

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.onTakePictureClicked_.bind(this));

  document.querySelector('#toolbar #album-enter').addEventListener('click',
      this.onAlbumEnterClicked_.bind(this));

  document.querySelector('#toggle-record').addEventListener(
      'click', this.onToggleRecordClicked_.bind(this));
  document.querySelector('#toggle-timer').addEventListener(
      'keypress', this.onToggleTimerKeyPress_.bind(this));
  document.querySelector('#toggle-timer').addEventListener(
      'click', this.onToggleTimerClicked_.bind(this));
  document.querySelector('#toggle-camera').addEventListener(
      'click', this.onToggleCameraClicked_.bind(this));
  document.querySelector('#toggle-mirror').addEventListener(
      'keypress', this.onToggleMirrorKeyPress_.bind(this));
  document.querySelector('#toggle-mirror').addEventListener(
      'click', this.onToggleMirrorClicked_.bind(this));

  // Load the shutter, tick, and recording sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
  this.tickSound_.src = '../sounds/tick.ogg';
  this.recordStartSound_.src = '../sounds/record_start.ogg';
  this.recordEndSound_.src = '../sounds/record_end.ogg';
};

camera.views.Camera.prototype = {
  __proto__: camera.View.prototype,
  get capturing() {
    return document.body.classList.contains('capturing');
  }
};

/**
 * Initializes the view. Call the callback on completion.
 * @param {function()} callback Completion callback.
 */
camera.views.Camera.prototype.initialize = function(callback) {
  // Select the default state of the toggle buttons.
  chrome.storage.local.get(
      {
        toggleTimer: false,
        toggleMirror: true,  // Deprecated.
        mirroringToggles: {},  // Per device.
      },
      function(values) {
        document.querySelector('#toggle-timer').checked =
            values.toggleTimer;
        this.legacyMirroringToggle_ = values.toggleMirror;
        this.mirroringToggles_ = values.mirroringToggles;
      }.bind(this));
  // Remove the deprecated values.
  chrome.storage.local.remove(['effectIndex', 'toggleMulti']);

  // TODO: Replace with "devicechanged" event once it's implemented in Chrome.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(this.maybeRefreshVideoDeviceIds_.bind(this), 1000);

  // Start the camera after refreshing the device ids.
  this.start_();

  // Monitor the locked state to avoid retrying camera connection when locked.
  chrome.idle.onStateChanged.addListener(function(newState) {
    if (newState == 'locked')
      this.locked_ = true;
    else if (newState == 'active')
      this.locked_ = false;
  }.bind(this));

  // Acquire the gallery model.
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
    this.model_.addObserver(this);
    this.updateAlbumButton_();
    callback();
  }.bind(this), function() {
    // TODO(yuli): Add error handling.
    callback();
  });
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
camera.views.Camera.prototype.onLeave = function() {
};

/**
 * @override
 */
camera.views.Camera.prototype.onActivate = function() {
  if (document.activeElement != document.body)
    document.querySelector('#take-picture').focus();
  this.updateAlbumButton_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onInactivate = function() {
  if (this.taking_) {
    this.endTakePicture_();
  }
};

/**
 * Handles clicking on the take-picture button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onTakePictureClicked_ = function(event) {
  if (this.is_recording_mode_ && this.mediaRecorder_ == null) {
    if (this.mediaRecorder_ == null) {
      // Create a media-recorder before proceeding to record video.
      this.mediaRecorder_ = this.createMediaRecorder_(this.stream_);
      if (this.mediaRecorder_ == null) {
        this.showToastMessage_(chrome.i18n.getMessage(
            'errorMsgRecordStartFailed'));
        return;
      }
    }
  } else {
    if (this.imageCapture_ == null) {
      // Create a image-capture before proceeding to take photo.
      const track = this.stream_ && this.stream_.getVideoTracks()[0];
      this.imageCapture_ = !track ? null : new ImageCapture(track);
      if (this.imageCapture_ == null) {
        this.showToastMessage_(chrome.i18n.getMessage(
            'errorMsgTakePhotoFailed'));
        return;
      }
    }
  }
  this.takePicture_();
};

/**
 * Handles clicking on the album button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onAlbumEnterClicked_ = function(event) {
  this.router.navigate(camera.Router.ViewIdentifier.ALBUM);
};

/**
 * Handles pressing a key on the timer switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerKeyPress_ = function(event) {
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-timer').click();
};

/**
 * Handles pressing a key on the mirror switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleMirrorKeyPress_ = function(event) {
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-mirror').click();
};

/**
 * Handles clicking on the timer switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerClicked_ = function(event) {
  var enabled = document.querySelector('#toggle-timer').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleTimerActiveMessage' :
                                       'toggleTimerInactiveMessage'));
  chrome.storage.local.set({toggleTimer: enabled});
};

/**
 * Handles clicking on the toggle camera switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleCameraClicked_ = function(event) {
  this.videoDeviceIds_.then(function(deviceIds) {
    var index = deviceIds.indexOf(this.videoDeviceId_);
    if (index == -1)
      index = 0;

    if (deviceIds.length > 0) {
      index = (index + 1) % deviceIds.length;
      this.videoDeviceId_ = deviceIds[index];
    }

    this.stop_();
  }.bind(this));
};

/**
 * Handles clicking on the video-recording switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleRecordClicked_ = function(event) {
  var label;
  var toggleRecord = document.querySelector('#toggle-record');
  if (this.is_recording_mode_) {
    this.is_recording_mode_ = false;
    toggleRecord.classList.remove('toggle-off');
    label = 'toggleRecordOnButton';
  } else {
    this.is_recording_mode_ = true;
    toggleRecord.classList.add('toggle-off');
    label = 'toggleRecordOffButton';
  }
  this.updateButtonLabel_(toggleRecord, label);

  var takePictureButton = document.querySelector('#take-picture');
  if (this.is_recording_mode_) {
    takePictureButton.classList.add('motion-picture');
    label = 'recordVideoStartButton';
  } else {
    takePictureButton.classList.remove('motion-picture');
    label = 'takePictureButton';
  }
  this.updateButtonLabel_(takePictureButton, label);

  document.querySelector('#toggle-timer').hidden = this.is_recording_mode_;

  this.stop_();
};

/**
 * Handles clicking on the mirror switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleMirrorClicked_ = function(event) {
  var enabled = document.querySelector('#toggle-mirror').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleMirrorActiveMessage' :
                                       'toggleMirrorInactiveMessage'));
  this.mirroringToggles_[this.videoDeviceId_] = enabled;
  chrome.storage.local.set({mirroringToggles: this.mirroringToggles_});
  this.updateMirroring_();
};

/**
 * Updates the UI to reflect the mirroring either set automatically or by user.
 * @private
 */
camera.views.Camera.prototype.updateMirroring_ = function() {
  var toggleMirror = document.querySelector('#toggle-mirror')
  var enabled;

  var track = this.stream_ && this.stream_.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  var facingMode = trackSettings && trackSettings.facingMode;

  toggleMirror.hidden = !!facingMode;

  if (facingMode) {
    // Automatic mirroring detection.
    enabled = facingMode == 'user';
  } else {
    // Manual mirroring.
    if (this.videoDeviceId_ in this.mirroringToggles_)
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    else
      enabled = this.legacyMirroringToggle_;
  }

  toggleMirror.checked = enabled;
  document.body.classList.toggle('mirror', enabled);
};

/**
 * Updates the album-button enabled/disabled UI for model changes.
 * @private
 */
camera.views.Camera.prototype.updateAlbumButton_ = function() {
  // Album-button would also be disabled if camera isn't capturing.
  document.querySelector('#album-enter').disabled =
      !this.model_ || this.model_.length == 0 || !this.capturing;
};

/**
 * Updates the toolbar enabled/disabled UI for capturing or taking-picture
 * state changes.
 * @private
 */
camera.views.Camera.prototype.updateToolbar_ = function() {
  var disabled = !this.capturing || this.taking_;
  document.querySelector('#toggle-timer').disabled = disabled;
  document.querySelector('#toggle-mirror').disabled = disabled;
  document.querySelector('#toggle-camera').disabled = disabled;
  document.querySelector('#toggle-record').disabled = disabled;
  document.querySelector('#take-picture').disabled = disabled;

  this.updateAlbumButton_();
};

/**
 * Updates the button's labels.
 *
 * @param {HTMLElement} button Button element to be updated.
 * @param {string} label Label to be set.
 * @private
 */
camera.views.Camera.prototype.updateButtonLabel_ = function(button, label) {
  button.setAttribute('i18n-label', label);
  button.setAttribute('aria-label', chrome.i18n.getMessage(label));
};

/**
 * Enables the audio track for audio capturing.
 *
 * @param {boolean} enabled True to enable audio track, false to disable.
 * @private
 */
camera.views.Camera.prototype.enableAudio_ = function(enabled) {
  var track = this.stream_ && this.stream_.getAudioTracks()[0];
  if (track) {
    track.enabled = enabled;
  }
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
    this.showVersion_();
    this.keyBuffer_ = '';
  }

  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Space':  // Space key for taking the picture.
      document.querySelector('#take-picture').click();
      event.stopPropagation();
      event.preventDefault();
      break;
    case 'G':  // G key for the gallery.
      document.querySelector('#album-enter').click();
      event.preventDefault();
      break;
  }
};

/**
 * Shows a non-intrusive toast message in the middle of the screen.
 * TODO(yuli): Add parameter to take i18n message name.
 * @param {string} message Message to be shown.
 * @private
 */
camera.views.Camera.prototype.showToastMessage_ = function(message) {
  var cancelHideTimer = function() {
    if (this.toastHideTimer_) {
      clearTimeout(this.toastHideTimer_);
      this.toastHideTimer_ = null;
    }
  }.bind(this);

  // If running, then reinvoke recursively after closing the toast message.
  if (this.toastEffect_.animating || this.toastHideTimer_) {
    cancelHideTimer();
    this.toastEffect_.invoke({
      visible: false
    }, this.showToastMessage_.bind(this, message));
    return;
  }

  // Cancel any pending hide timers.
  cancelHideTimer();

  // Start the hide timer.
  this.toastHideTimer_ = setTimeout(function() {
    this.toastEffect_.invoke({
      visible: false
    }, function() {});
    this.toastHideTimer_ = null;
  }.bind(this), 2000);

  // Show the toast message.
  this.toastEffect_.invoke({
    visible: true,
    message: message
  }, function() {});
};

/**
 * Starts to show the timer of elapsed recording time.
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
  }

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
 * Shows a version dialog.
 * @private
 */
camera.views.Camera.prototype.showVersion_ = function() {
  // No need to localize, since for debugging purpose only.
  var message = 'Version: ' + chrome.runtime.getManifest().version + '\n' +
      'Resolution: ' +
          this.video_.videoWidth + 'x' + this.video_.videoHeight + '\n';
  this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
    type: camera.views.Dialog.Type.ALERT,
    message: message
  });
};

/**
 * Takes the picture (maybe with a timer if enabled); or ends an ongoing
 * recording started from the prior takePicture_() call.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (this.taking_) {
    // End the prior ongoing taking/recording; a new taking/reocording cannot be
    // started until the prior one is ended.
    this.endTakePicture_();
    return;
  }

  this.taking_ = true;
  this.updateToolbar_();

  var toggleTimer = document.querySelector('#toggle-timer');
  var tickCounter = (!toggleTimer.hidden && toggleTimer.checked) ? 6 : 1;
  var onTimerTick = function() {
    tickCounter--;
    if (tickCounter == 0) {
      if (this.is_recording_mode_) {
        // Play a sound before recording started and wait for its playback to
        // avoid it being recorded.
        this.recordStartSound_.currentTime = 0;
        this.recordStartSound_.play();
        this.doTakePicture_(250);
      } else {
        this.doTakePicture_(0);
      }
    } else {
      this.tickTakePictureTimer_ = setTimeout(onTimerTick, 1000);
      this.tickSound_.play();
      // Blink the toggle timer button.
      toggleTimer.classList.add('animate');
      setTimeout(function() {
        if (this.tickTakePictureTimer_)
          toggleTimer.classList.remove('animate');
      }.bind(this), 500);
    }
  }.bind(this);

  // First tick immediately in the next message loop cycle.
  this.tickTakePictureTimer_ = setTimeout(onTimerTick, 0);
};

/**
 * Ends ongoing recording or clears scheduled further picture takes (if any).
 * @private
 */
camera.views.Camera.prototype.endTakePicture_ = function() {
  if (this.doTakePictureTimer_) {
    clearTimeout(this.doTakePictureTimer_);
    this.doTakePictureTimer_ = null;
  }
  if (this.tickTakePictureTimer_) {
    clearTimeout(this.tickTakePictureTimer_);
    this.tickTakePictureTimer_ = null;
  }
  // Toolbar will be updated after taking-photo/recording-video is finished.
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }
  document.querySelector('#toggle-timer').classList.remove('animate');
};

/**
 * Takes the photo or starts to record video after timeout, and saves the photo
 * or video-recording to the model.
 * @param {number} timeout Timeout in milliseconds.
 * @private
 */
camera.views.Camera.prototype.doTakePicture_ = function(timeout) {
  this.doTakePictureTimer_ = setTimeout(function() {
    var errorToast = function(msg) {
      this.showToastMessage_(chrome.i18n.getMessage(msg));
    }.bind(this);

    // Add picture to the model.
    var addPicture = function(blob, type) {
      var saveFailure = function() {
        errorToast('errorMsgGallerySaveFailed');
      };
      if (!this.model_) {
        saveFailure();
        return;
      }
      var albumButton = document.querySelector('#toolbar #album-enter');
      camera.util.setAnimationClass(albumButton, albumButton, 'flash');

      this.model_.addPicture(blob, type, saveFailure);
    }.bind(this);

    // Update toolbar after finishing taking-photo/recording-video.
    var onFinish = function() {
      this.taking_ = false;
      this.updateToolbar_();
    }.bind(this);

    if (this.is_recording_mode_) {
      var takePictureButton = document.querySelector('#take-picture');

      var stopRecordingUI = function() {
        this.hideRecordingTimer_();
        takePictureButton.classList.remove('flash');
        this.updateButtonLabel_(takePictureButton, 'recordVideoStartButton');
      }.bind(this);

      this.createBlobOnRecordStop_(function(blob) {
        // Play a sound for a successful recording after recording stopped.
        this.recordEndSound_.currentTime = 0;
        this.recordEndSound_.play();
        stopRecordingUI();
        addPicture(blob, camera.models.Gallery.PictureType.MOTION);
      }.bind(this), function() {
        stopRecordingUI();
        // The recording may have no data available because it's too short or
        // the media recorder is not stable and Chrome needs to restart.
        errorToast('errorMsgEmptyRecording');
      }, onFinish);

      // Re-enable the take-picture button to stop recording later and flash
      // the take-picture button until the recording is stopped.
      takePictureButton.disabled = false;
      this.updateButtonLabel_(takePictureButton, 'recordVideoStopButton');
      takePictureButton.classList.add('flash');
      this.showRecordingTimer_();
    } else {
      this.createBlobOnPhotoTake_(function(blob) {
        // Play a shutter sound.
        this.shutterSound_.currentTime = 0;
        this.shutterSound_.play();
        addPicture(blob, camera.models.Gallery.PictureType.STILL);
      }.bind(this), function() {
        errorToast('errorMsgTakePhotoFailed');
      }, onFinish);
    }
  }.bind(this), timeout);
};

/**
 * Starts a recording and creates a blob of it after the recorder is stopped.
 * @param {function(Blob)} onSuccess Success callback with the recorded video
 *     as a blob.
 * @param {function()} onFailure Failure callback.
 * @param {function()} onFinish Finish callback.
 * @private
 */
camera.views.Camera.prototype.createBlobOnRecordStop_ = function(
    onSuccess, onFailure, onFinish) {
  var recordedChunks = [];
  this.mediaRecorder_.ondataavailable = function(event) {
    if (event.data && event.data.size > 0) {
      recordedChunks.push(event.data);
    }
  }.bind(this);

  this.mediaRecorder_.onstop = function(event) {
    this.enableAudio_(false);
    // TODO(yuli): Handle insufficient storage.
    var recordedBlob = new Blob(recordedChunks, {type: 'video/webm'});
    recordedChunks = [];
    if (recordedBlob.size) {
      onSuccess(recordedBlob);
    } else {
      onFailure();
    }
    onFinish();
  }.bind(this);

  // Start recording.
  this.enableAudio_(true);
  this.mediaRecorder_.start();
};

/**
 * Takes a photo and creates a blob of it.
 * @param {function(Blob)} onSuccess Success callback with the taken photo as a
 *     blob.
 * @param {function()} onFailure Failure callback.
 * @param {function()} onFinish Finish callback.
 * @private
 */
camera.views.Camera.prototype.createBlobOnPhotoTake_ = function(
    onSuccess, onFailure, onFinish) {
  // Since the takePhoto() API has not been implemented on CrOS until 68, use
  // canvas-drawing as a fallback for older Chrome versions.
  // TODO(shenghao): Remove canvas-drawing after min-chrome-version is above 68.
  if (camera.util.isChromeVersionAbove(68)) {
    let getPhotoCapabilities = function() {
      if (this.photoCapabilities_ == null) {
         this.photoCapabilities_ =
            this.imageCapture_.getPhotoCapabilities();
      }
      return this.photoCapabilities_;
    }.bind(this);

    getPhotoCapabilities().then(photoCapabilities => {
      // Set to take the highest resolution, but the photo to be taken will
      // still have the same aspect ratio with the preview.
      var photoSettings = {
          imageWidth: photoCapabilities.imageWidth.max,
          imageHeight: photoCapabilities.imageHeight.max
      };
      return this.imageCapture_.takePhoto(photoSettings);
    }).then(onSuccess).catch(error => { onFailure(); }).finally(onFinish);
  } else {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    canvas.width = this.video_.videoWidth;
    canvas.height = this.video_.videoHeight;
    context.drawImage(this.video_, 0, 0);
    canvas.toBlob(blob => {
      onSuccess(blob);
      onFinish();
    }, 'image/jpeg');
  }
};

/**
 * Creates the media recorder for the video stream.
 *
 * @param {MediaStream} stream Media stream to be recorded.
 * @return {MediaRecorder} Media recorder created.
 * @private
 */
camera.views.Camera.prototype.createMediaRecorder_ = function(stream) {
  if (!window.MediaRecorder) {
    return null;
  }

  // Webm with H264 is the only preferred MediaRecorder video recording format.
  var type = 'video/webm; codecs=h264';
  if (!MediaRecorder.isTypeSupported(type)) {
    console.error('MediaRecorder does not support mimeType: ' + type);
    return null;
  }
  try {
    var options = {mimeType: type};
    return new MediaRecorder(stream, options);
  } catch (e) {
    console.error('Unable to create MediaRecorder: ' + e + '. mimeType: ' +
        type);
    return null;
  }
};

/**
 * Updates the video device id by the constraints or by the acquired stream.
 *
 * @param {!Object} constraints Constraints that acquires the current stream.
 * @private
 */
camera.views.Camera.prototype.updateVideoDeviceId_ = function(constraints) {
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
    var track = this.stream_.getVideoTracks()[0];
    var trackSettings = track.getSettings && track.getSettings();
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
  }
};

/**
 * Starts capturing with the specified constraints.
 *
 * @param {!Object} constraints Constraints passed to WebRTC.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback, eg. the constraints are
 *     not supported.
 * @private
 */
 camera.views.Camera.prototype.startWithConstraints_ =
     function(constraints, onSuccess, onFailure) {
  navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
    if (this.is_recording_mode_) {
      // Disable audio stream until video recording is started.
      this.enableAudio_(false);
    }
    // Mute to avoid echo from the captured audio.
    this.video_.muted = true;
    this.video_.srcObject = stream;
    var onLoadedMetadata = function() {
      this.video_.removeEventListener('loadedmetadata', onLoadedMetadata);
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog_ = setInterval(function() {
        // Check if video stream is ended (audio stream may still be live).
        if (!stream.getVideoTracks().length ||
            stream.getVideoTracks()[0].readyState == 'ended') {
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
          if (this.taking_) {
            this.endTakePicture_();
          }
          this.mediaRecorder_ = null;
          this.imageCapture_ = null;
          this.photoCapabilities_ = null;
          // Try reconnecting the camera to capture new streams.
          document.body.classList.remove('capturing');
          this.updateToolbar_();
          this.stream_ = null;
          this.start_();
        }
      }.bind(this), 100);

      this.stream_ = stream;
      document.body.classList.add('capturing');
      this.updateWindowSize_();
      this.updateToolbar_();
      this.updateVideoDeviceId_(constraints);
      this.updateMirroring_();
      onSuccess();
    }.bind(this);
    // Load the stream and wait for the metadata.
    this.video_.addEventListener('loadedmetadata', onLoadedMetadata);
    this.video_.play();
  }.bind(this), function(error) {
    if (error && error.name != 'ConstraintNotSatisfiedError') {
      // Constraint errors are expected, so don't report them.
      console.error(error);
    }
    onFailure();
  });
};

/**
 * Sets the window size to match the video frame aspect ratio.
 * @private
 */
camera.views.Camera.prototype.updateWindowSize_ = function() {
  var appWindow = chrome.app.window.current();
  if (appWindow.isMaximized() || appWindow.isFullscreen())
    return;

  // Minimize window dimension changes to avoid the jarring movement.
  var outer = appWindow.outerBounds;
  var inner = appWindow.innerBounds;
  var aspectRatio = this.video_.videoWidth / this.video_.videoHeight;
  var scaleW = inner.width / this.video_.videoWidth;
  var scaleH = inner.height / this.video_.videoHeight;
  var innerW = this.video_.videoWidth * scaleH;
  var innerH = this.video_.videoHeight * scaleW;
  if (Math.abs(innerW - inner.width) <= Math.abs(innerH - inner.height)) {
    // Keep the original height and use the calculated new width.
    innerH = inner.height;
  } else {
    // Keep the original width and use the calculated new height.
    innerW = inner.width;
  }

  // Make the window not larger than the screen available width/height.
  var diffW = outer.width - inner.width;
  var diffH = outer.height - inner.height;
  var maxInnerW = screen.availWidth - diffW;
  var maxInnerH = screen.availHeight - diffH;
  if (innerW > maxInnerW || innerH > maxInnerH) {
    var scale = Math.min(maxInnerH / innerH, maxInnerW / innerW);
    innerW = scale * innerW;
    innerH = scale * innerH;
  }

  // Keep the window centered at its original position and inside the screen.
  var outerW = Math.round(innerW + diffW);
  var outerH = Math.round(innerH + diffH);
  var outerLeft = Math.round(outer.left - (outerW - outer.width) / 2);
  var outerTop = Math.round(outer.top - (outerH - outer.height) / 2);

  outerLeft = Math.max(screen.availLeft, outerLeft);
  outerLeft = Math.min(
      screen.availLeft + screen.availWidth - outerW, outerLeft);
  outerTop = Math.max(0, outerTop);
  outerTop = Math.min(
      screen.availTop + screen.availHeight - outerH, outerTop);

  outer.setSize(outerW, outerH);
  outer.setPosition(outerLeft, outerTop);
};

/**
 * Updates the video element's size for previewing in the window.
 * @private
 */
camera.views.Camera.prototype.updateVideoSize_ = function() {
  if (!this.video_.videoHeight)
    return;

  // Keep the video frame aspect ratio and fill up the whole window inner width
  // and height.
  // TODO(yuli): Handle letterbox mode when the window is fullscreen/maximized.
  var scale = Math.max(window.innerHeight / this.video_.videoHeight,
      window.innerWidth / this.video_.videoWidth);
  this.video_.width = scale * this.video_.videoWidth;
  this.video_.height = scale * this.video_.videoHeight;
}

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
camera.views.Camera.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_)
    return;

  this.refreshingVideoDeviceIds_ = true;
  this.videoDeviceIds_ = this.collectVideoDevices_();

  // Update the UI.
  this.videoDeviceIds_.then(function(devices) {
    document.querySelector('#toggle-camera').hidden = devices.length < 2;
  }, function() {
    document.querySelector('#toggle-camera').hidden = true;
  }).then(function() {
    this.refreshingVideoDeviceIds_ = false;
  }.bind(this));
};

/**
 * Collects all of the available video input devices.
 * @return {!Promise<!Array<string>}
 * @private
 */
camera.views.Camera.prototype.collectVideoDevices_ = function() {
  return navigator.mediaDevices.enumerateDevices().then(function(devices) {
      var availableVideoDevices = [];
      devices.forEach(function(device) {
        if (device.kind != 'videoinput')
          return;
        availableVideoDevices.push(device.deviceId);
      });
      return availableVideoDevices;
    });
};

/**
 * Sorts the video device ids by the current video device id.
 * @return {!Promise<!Array<string>}
 * @private
 */
camera.views.Camera.prototype.sortVideoDeviceIds_ = function() {
  return this.videoDeviceIds_.then(function(deviceIds) {
    if (deviceIds.length == 0) {
      throw 'Device list empty.';
    }

    // Put the preferred camera first.
    var sortedDeviceIds = deviceIds.slice(0).sort(function(a, b) {
      if (a == b)
        return 0;
      if (a == this.videoDeviceId_)
        return -1;
      else
        return 1;
    }.bind(this));

    // Prepended 'null' deviceId means the system default camera. Add it only
    // when the app is launched (no video-device-id set).
    if (this.videoDeviceId_ == null) {
        sortedDeviceIds.unshift(null);
    }
    return sortedDeviceIds;
  }.bind(this));
};

/**
 * Stops the camera stream so it retries opening the camera stream on new
 * device or with new constraints.
 */
camera.views.Camera.prototype.stop_ = function() {
  // TODO(mtomasz): Prevent blink. Clear somehow the video tag.
  if (this.stream_)
    this.stream_.getVideoTracks()[0].stop();
};

/**
 * Returns constraints-candidates with the specified device id.
 * @param {string} deviceId Device id to be set in the constraints.
 * @return {Array<Object>}
 * @private
 */
camera.views.Camera.prototype.constraintsCandidates_ = function(deviceId) {
  var videoConstraints = function() {
    if (this.is_recording_mode_) {
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
  }.bind(this);

  return videoConstraints().map(function(videoConstraint) {
        // Each passed-in video-constraint will be modified here.
        if (deviceId) {
          videoConstraint.deviceId = { exact: deviceId };
        } else {
          // As a default camera use the one which is facing the user.
          videoConstraint.facingMode = { exact: 'user' };
        }
        return { audio: this.is_recording_mode_, video: videoConstraint };
      }.bind(this));
};

/**
 * Starts capturing the camera with the highest possible resolution.
 * Can be called only once.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var scheduleRetry = function() {
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.retryStartTimer_ = setTimeout(this.start_.bind(this), 100);
  }.bind(this);

  if (this.locked_) {
    scheduleRetry();
    return;
  }

  var onSuccess = function() {
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    // Remove the error layer if any.
    this.context_.onErrorRecovered('no-camera');

    this.showToastMessage_(chrome.i18n.getMessage(this.is_recording_mode_ ?
        'recordVideoActiveMessage' : 'takePictureActiveMessage'));
  }.bind(this);

  var onFailure = function() {
    this.context_.onError(
        'no-camera',
        chrome.i18n.getMessage('errorMsgNoCamera'),
        chrome.i18n.getMessage('errorMsgNoCameraHint'));
    scheduleRetry();
  }.bind(this);

  var constraintsCandidates = [];

  var tryStartWithConstraints = function(index) {
    if (this.locked_) {
      scheduleRetry();
      return;
    }
    if (index >= constraintsCandidates.length) {
      onFailure();
      return;
    }
    this.startWithConstraints_(
        constraintsCandidates[index], onSuccess, function() {
          // TODO(mtomasz): Workaround for crbug.com/383241.
          setTimeout(tryStartWithConstraints.bind(this, index + 1), 0);
        });
  }.bind(this);

  this.sortVideoDeviceIds_().then(function(sortedDeviceIds) {
    sortedDeviceIds.forEach(function(deviceId) {
      constraintsCandidates = constraintsCandidates.concat(
          this.constraintsCandidates_(deviceId));
    }.bind(this));

    tryStartWithConstraints(0);
  }.bind(this)).catch(function(error) {
    console.error('Failed to start camera.', error);
    onFailure();
  });
};

/**
 * @override
 */
camera.views.Camera.prototype.onPictureDeleting = function(picture) {
  // No-op here (right before deleting a picure) as no picture would be deleted
  // when camera view is active and onActivate() already handles updating the
  // album-button.
};

/**
 * @override
 */
camera.views.Camera.prototype.onPictureAdded = function(picture) {
  this.updateAlbumButton_();
};

