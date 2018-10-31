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
 * Creates a controller for the video preview of Camera view.
 * @param {function()} onNewStreamNeeded Callback to request new stream.
 * @param {function(number)} onAspectRatio Callback to report aspect ratio.
 * @constructor
 */
camera.views.camera.Preview = function(onNewStreamNeeded, onAspectRatio) {
  /**
   * @type {function()}
   * @private
   */
  this.onNewStreamNeeded_ = onNewStreamNeeded;

  /**
   * @type {function(number)}
   * @private
   */
  this.onAspectRatio_ = onAspectRatio;

  /**
   * Video element to capture the stream.
   * @type {Video}
   * @private
   */
  this.video_ = document.querySelector('#preview-video');

  /**
   * Current active stream.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * Watchdog for stream-end.
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Promise for the current applying focus.
   * @type {Promise<>}
   * @private
   */
  this.focus_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  this.video_.cleanup = () => {};
};

camera.views.camera.Preview.prototype = {
  get stream() {
    return this.stream_;
  },
};

/**
 * @override
 */
camera.views.camera.Preview.prototype.toString = function() {
  return this.video_.videoHeight ?
      (this.video_.videoWidth + ' x ' + this.video_.videoHeight) : '';
};

/**
 * Sets video element's source.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise<>} Promise for the operation.
 */
camera.views.camera.Preview.prototype.setSource_ = function(stream) {
  return new Promise((resolve) => {
    var video = document.createElement('video');
    video.id = 'preview-video';
    video.setAttribute('aria-hidden', 'true');

    var onLoadedMetadata = () => {
      var onIntrinsicSize = () => {
        // Handles the intrinsic size first fetched or its orientation changes.
        if (this.video_.videoWidth && this.video_.videoHeight) {
          this.onAspectRatio_(this.video_.videoWidth / this.video_.videoHeight);
        }
        this.cancelFocus_();
      };
      var onClick = (event) => {
        this.applyFocus_(event.offsetX, event.offsetY);
      };
      video.removeEventListener('loadedmetadata', onLoadedMetadata);
      video.addEventListener('resize', onIntrinsicSize);
      video.addEventListener('click', onClick);
      video.cleanup = () => {
        video.removeEventListener('resize', onIntrinsicSize);
        video.removeEventListener('click', onClick);
        video.removeAttribute('srcObject');
        video.load();
      };
      video.play();
      this.video_.parentElement.replaceChild(video, this.video_);
      this.video_.cleanup();
      this.video_ = video;
      onIntrinsicSize();
      resolve();
    };
    video.addEventListener('loadedmetadata', onLoadedMetadata);
    video.muted = true;  // Mute to avoid echo from the captured audio.
    video.srcObject = stream;
  });
};

/**
 * Starts the preview with the source stream.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise<>} Promise for the operation.
 */
camera.views.camera.Preview.prototype.start = function(stream) {
  return this.setSource_(stream).then(() => {
    // Use a watchdog since the stream.onended event is unreliable in the
    // recent version of Chrome. As of 55, the event is still broken.
    this.watchdog_ = setInterval(() => {
      // Check if video stream is ended (audio stream may still be live).
      if (!stream.getVideoTracks().length ||
          stream.getVideoTracks()[0].readyState == 'ended') {
        clearInterval(this.watchdog_);
        this.watchdog_ = null;
        this.stream_ = null;
        this.onNewStreamNeeded_();
      }
    }, 100);
    this.stream_ = stream;
  });
};

/**
 * Stops the preview.
 */
camera.views.camera.Preview.prototype.stop = function() {
  if (this.watchdog_) {
    clearInterval(this.watchdog_);
    this.watchdog_ = null;
  }
  // Pause video element to avoid black frames during transition.
  this.video_.pause();
  if (this.stream_) {
    this.stream_.getVideoTracks()[0].stop();
    this.stream_ = null;
  }
};

/**
 * Creates an image blob of the current frame.
 * @return {!Promise<Blob>} Promise for the result.
 */
camera.views.camera.Preview.prototype.toImage = function() {
  var canvas = document.createElement('canvas');
  var ctx = canvas.getContext('2d');
  canvas.width = this.video_.videoWidth;
  canvas.height = this.video_.videoHeight;
  ctx.drawImage(this.video_, 0, 0);
  return new Promise((resolve, reject) => {
    canvas.toBlob(blob => {
      if (blob) {
        resolve(blob);
      } else {
        reject('Photo blob error.');
      }
    }, 'image/jpeg');
  });
};

/**
 * Applies focus at the given coordinate.
 * @param {number} offsetX X-coordinate based on the video element.
 * @param {number} offsetY Y-coordinate based on the video element.
 * @private
 */
camera.views.camera.Preview.prototype.applyFocus_ = function(offsetX, offsetY) {
  this.cancelFocus_();

  // Normalize to square space coordinates by W3C spec.
  var x = offsetX / this.video_.width;
  var y = offsetY / this.video_.height;
  var constraints = {advanced: [{pointsOfInterest: [{x, y}]}]};
  var track = this.video_.srcObject.getVideoTracks()[0];
  var focus = track.applyConstraints(constraints).then(() => {
    if (focus != this.focus_) {
      throw 'Focus was cancelled.';
    }
    var aim = document.querySelector('#preview-focus-aim');
    var clone = aim.cloneNode(true);
    clone.style.left = `${x * 100}%`;
    clone.style.top = `${y * 100}%`;
    clone.hidden = false;
    aim.parentElement.replaceChild(clone, aim);
  }).catch(error => console.error(error));
  this.focus_ = focus;
};

/**
 * Cancels the current applying focus.
 * @private
 */
camera.views.camera.Preview.prototype.cancelFocus_ = function() {
  this.focus_ = null;
  document.querySelector('#preview-focus-aim').hidden = true;
};
