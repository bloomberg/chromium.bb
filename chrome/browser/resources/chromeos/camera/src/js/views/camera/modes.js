// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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
 * Mode controller managing capture sequence of different camera mode.
 * @param {function()} doSwitchMode Callback to trigger mode switching.
 * @param {function(?Blob, boolean, string): Promise} doSavePicture Callback for
 *     saving picture.
 * @constructor
 */
cca.views.camera.Modes = function(doSwitchMode, doSavePicture) {
  /**
   * @type {function()}
   * @private
   */
  this.doSwitchMode_ = doSwitchMode;

  /**
   * Callback for saving picture.
   * @type {function(?Blob, boolean, string): Promise}
   * @private
   */
  this.doSavePicture_ = doSavePicture;

  /**
   * Capture controller of current camera mode.
   * @type {cca.views.camera.Mode}
   */
  this.current = null;

  /**
   * Stream of current mode.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * Classname and factory function of available modes.
   * @type {Object<string, function(): cca.views.camera.Mode>}
   * @private
   */
  this.modeFactories_ = {
    'photo-mode': () =>
        new cca.views.camera.Photo(this.stream_, this.doSavePicture_),
    'video-mode': () =>
        new cca.views.camera.Video(this.stream_, this.doSavePicture_),
  };

  // End of properties, seal the object.
  Object.seal(this);

  [['#switch-recordvideo', 'video-mode'], ['#switch-takephoto', 'photo-mode']]
      .forEach(
          ([selector, mode]) =>
              document.querySelector(selector).addEventListener(
                  'click', this.switchMode_.bind(this, mode)));

  // Set default mode when app started.
  cca.state.set('photo-mode', true);
};

/**
 * Switches mode to either video-recording or photo-taking.
 * @param {string} mode Class name of the switching mode.
 * @private
 */
cca.views.camera.Modes.prototype.switchMode_ = function(mode) {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }
  Object.keys(this.modeFactories_).forEach((m) => cca.state.set(m, m == mode));
  cca.state.set('mode-switching', true);
  this.doSwitchMode_().then(() => cca.state.set('mode-switching', false));
};

/**
 * Creates and updates new current mode object.
 * @async
 * @param {MediaStream} stream Stream of the new switching mode.
 */
cca.views.camera.Modes.prototype.update = async function(stream) {
  if (this.current != null) {
    await this.current.stopCapture();
  }
  this.stream_ = stream;
  var mode = Object.keys(this.modeFactories_).find(cca.state.get);
  this.current = this.modeFactories_[mode]();
};

/**
 * Base class for controlling capture sequence in different camera modes.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @constructor
 */
cca.views.camera.Mode = function(stream, doSavePicture) {
  /**
   * Promise for ongoing capture operation.
   * @type {?Promise}
   * @private
   */
  this.capture_ = null;

  /**
   * Stream of current mode.
   * @type {?Promise}
   * @protected
   */
  this.stream_ = stream;

  /**
   * Callback for saving picture.
   * @type {function(?Blob, boolean, string): Promise}
   * @protected
   */
  this.doSavePicture_ = doSavePicture;
};

/**
 * Initiates video/photo capture operation.
 * @return {?Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.startCapture = function() {
  if (!this.capture_) {
    this.capture_ = this.start_().finally(() => this.capture_ = null);
  }
  return this.capture_;
};

/**
 * Stops the ongoing capture operation.
 * @async
 * @return {Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.stopCapture = async function() {
  this.stop_();
  return await this.capture_;
};

/**
 * Initiates video/photo capturing operation under this mode.
 * @async
 * @protected
 */
cca.views.camera.Mode.prototype.start_ = async function() {};

/**
 * Stops the ongoing capture operation under this mode.
 * @protected
 */
cca.views.camera.Mode.prototype.stop_ = function() {};

/**
 * Video mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @constructor
 */
cca.views.camera.Video = function(stream, doSavePicture) {
  cca.views.camera.Mode.call(this, stream, doSavePicture);

  /**
   * Promise for play start sound delay.
   * @type {?Promise}
   * @private
   */
  this.startSound_ = null;

  /**
   * MediaRecorder object to record motion pictures.
   * @type {MediaRecorder}
   * @private
   */
  this.mediaRecorder_ = null;

  /**
   * Record-time for the elapsed recording time.
   * @type {cca.views.camera.RecordTime}
   * @private
   */
  this.recordTime_ = new cca.views.camera.RecordTime();

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.camera.Video.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Video.prototype.start_ = async function() {
  this.startSound_ = cca.sound.play('#sound-rec-start');
  try {
    await this.startSound_;
  } finally {
    this.startSound_ = null;
  }

  if (this.mediaRecorder_ == null) {
    try {
      if (!MediaRecorder.isTypeSupported(
              cca.views.camera.Video.VIDEO_MIMETYPE)) {
        throw new Error('The preferred mimeType is not supported.');
      }
      this.mediaRecorder_ = new MediaRecorder(
          this.stream_, {mimeType: cca.views.camera.Video.VIDEO_MIMETYPE});
    } catch (e) {
      cca.toast.show('error_msg_record_start_failed');
      throw e;
    }
  }

  let blob;
  // Take of recording will be ended by another shutter click.
  try {
    blob = await this.createVideoBlob_();
  } catch (e) {
    cca.toast.show('error_msg_empty_recording');
    throw e;
  }
  cca.sound.play('#sound-rec-end');

  await this.doSavePicture_(
      blob, true, (new cca.models.Filenamer()).newVideoName());
};

/**
 * @override
 */
cca.views.camera.Video.prototype.stop_ = function() {
  if (this.startSound_ && this.startSound_.cancel) {
    this.startSound_.cancel();
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }
};

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred
 * format.
 * @type {string}
 * @const
 */
cca.views.camera.Video.VIDEO_MIMETYPE = 'video/x-matroska;codecs=avc1';

/**
 * Starts a recording to create a blob of it after the recorder is stopped.
 * @return {!Promise<!Blob>} Promise for the result.
 * @private
 */
cca.views.camera.Video.prototype.createVideoBlob_ = function() {
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
          recordedChunks, {type: cca.views.camera.Video.VIDEO_MIMETYPE});
      recordedBlob.mins = this.recordTime_.stop();
      recordedChunks = [];
      if (recordedBlob.size) {
        resolve(recordedBlob);
      } else {
        reject(new Error('Video blob error.'));
      }
    };
    this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
    this.mediaRecorder_.addEventListener('stop', onstop);

    // Start recording and update the UI for the ongoing recording.
    // TODO(yuli): Don't re-enable audio after crbug.com/878255 fixed in M73.
    var track = this.stream_.getAudioTracks()[0];
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
 * Photo mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @constructor
 */
cca.views.camera.Photo = function(stream, doSavePicture) {
  cca.views.camera.Mode.call(this, stream, doSavePicture);

  /**
   * ImageCapture object to capture still photos.
   * @type {ImageCapture}
   * @private
   */
  this.imageCapture_ = null;
};

cca.views.camera.Photo.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Photo.prototype.start_ = async function() {
  if (this.imageCapture_ == null) {
    try {
      this.imageCapture_ = new ImageCapture(this.stream_.getVideoTracks()[0]);
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  let blob;
  try {
    blob = await this.createPhotoBlob_();
  } catch (e) {
    cca.toast.show('error_msg_take_photo_failed');
    throw e;
  }
  cca.sound.play('#sound-shutter');
  await this.doSavePicture_(
      blob, false, (new cca.models.Filenamer()).newImageName());
};

/**
 * Takes a photo to create a blob of it.
 * @async
 * @return {!Promise<!Blob>} Result blob.
 * @private
 */
cca.views.camera.Photo.prototype.createPhotoBlob_ = async function() {
  var photoCapabilities = await this.imageCapture_.getPhotoCapabilities();
  // Set to take the highest resolution, but the photo to be taken will
  // still have the same aspect ratio with the preview.
  var photoSettings = {
    imageWidth: photoCapabilities.imageWidth.max,
    imageHeight: photoCapabilities.imageHeight.max,
  };
  return await this.imageCapture_.takePhoto(photoSettings);
};

/**
 * Square mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @constructor
 */
cca.views.camera.Square = function(stream, doSavePicture) {
  cca.views.camera.Photo.call(this, stream, doSavePicture);

  /**
   * Picture saving callback from parent.
   * @type {function(?Blob, boolean, string): Promise}
   * @private
   */
  this.doAscentSave_ = this.doSavePicture_;

  // End of properties, seal the object.
  Object.seal(this);

  this.doSavePicture_ = async (blob, ...args) => {
    if (blob) {
      blob = await this.cropSquare(blob);
    }
    await this.doAscentSave_(blob, ...args);
  };
};

cca.views.camera.Square.prototype = {
  __proto__: cca.views.camera.Photo.prototype,
};

/**
 * Crops out maximum possible centered square from the image blob.
 * @param {Blob} blob
 * @return {Promise<Blob>} Promise with result cropped square image.
 */
cca.views.camera.Square.prototype.cropSquare = function(blob) {
  return new Promise((resolve, reject) => {
    var img = new Image();
    img.onload = () => {
      let side = Math.min(img.width, img.height);
      let canvas = document.createElement('canvas');
      canvas.width = side;
      canvas.height = side;
      let ctx = canvas.getContext('2d');
      ctx.drawImage(
          img, Math.floor((img.width - side) / 2),
          Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
      try {
        canvas.toBlob(resolve, 'image/jpeg');
      } catch (e) {
        reject(e);
      }
    };
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
};
