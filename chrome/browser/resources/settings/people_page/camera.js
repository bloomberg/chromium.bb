// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-camera' is the Polymer control used to take a picture from the
 * user webcam to use as a ChromeOS profile picture.
 *
 * @group Chrome Settings Elements
 * @element settings-camera
 */
(function() {

/**
 * Dimensions for camera capture.
 * @const
 */
var CAPTURE_SIZE = {
  height: 480,
  width: 480
};

/**
 * The image URL is set to this default value before the user has taken a photo.
 * @const
 */
var DEFAULT_IMAGE_URL = 'chrome://theme/IDR_BUTTON_USER_IMAGE_TAKE_PHOTO';

Polymer({
  is: 'settings-camera',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * True if the user has selected the camera as the user image source.
     * @type {boolean}
     */
    cameraActive: {
      type: Boolean,
      observer: 'cameraActiveChanged_',
    },

    /**
     * Title of the camera image. Defaults to the 'takePhoto' internationalized
     * string. Can be 'photoFromCamera' if the user has just taken a photo.
     * @type {string}
     */
    cameraTitle: {
      type: String,
      notify: true,
      value: function() { return this.i18n('takePhoto'); },
    },

    /**
     * A preview image suitable for displaying in an icon grid. If it is the
     * captured image, it has already been flipped.
     * @type {string}
     */
    previewImage: {
      type: String,
      notify: true,
      value: DEFAULT_IMAGE_URL,
    },

    /**
     * The data URL of the newly captured image.
     * @private {string}
     */
    imageUrl_: {
      type: String,
      value: '',
    },

    /**
     * The data URL of the newly captured image flipped over the x axis.
     * @private {string}
     */
    flippedImageUrl_: {
      type: String,
      value: '',
    },

    /**
     * True when the camera is in live mode (i.e. no still photo selected).
     * @private {boolean}
     */
    cameraLive_: {
      type: Boolean,
      value: true,
    },

    /**
     * True when the camera is actually streaming video. May be false even when
     * the camera is present and shown, but still initializing.
     * @private {boolean}
     */
    cameraOnline_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the photo is currently marked flipped.
     * @private {boolean}
     */
    isFlipped_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  attached: function() {
    this.$.cameraVideo.addEventListener('canplay', function() {
      this.cameraOnline_ = true;
    }.bind(this));
  },

  /**
   * Observer for the cameraActive property.
   * @private
   */
  cameraActiveChanged_: function() {
    if (this.cameraActive)
      this.startCamera_();
    else
      this.stopCamera_();
  },

  /**
   * Tries to start the camera stream capture.
   * @private
   */
  startCamera_: function() {
    this.stopCamera_();
    this.cameraStartInProgress_ = true;

    var successCallback = function(stream) {
      if (this.cameraStartInProgress_) {
        this.$.cameraVideo.src = URL.createObjectURL(stream);
        this.cameraStream_ = stream;
      } else {
        this.stopVideoTracks_(stream);
      }
      this.cameraStartInProgress_ = false;
    }.bind(this);

    var errorCallback = function() {
      this.cameraOnline_ = false;
      this.cameraStartInProgress_ = false;
    }.bind(this);

    navigator.webkitGetUserMedia({video: true}, successCallback, errorCallback);
  },

  /**
   * Stops camera capture, if it's currently cameraActive.
   * @private
   */
  stopCamera_: function() {
    this.cameraOnline_ = false;
    this.$.cameraVideo.src = '';
    if (this.cameraStream_)
      this.stopVideoTracks_(this.cameraStream_);
    // Cancel any pending getUserMedia() checks.
    this.cameraStartInProgress_ = false;
  },

  /**
   * Stops all video tracks associated with a MediaStream object.
   * @param {!MediaStream} stream
   * @private
   */
  stopVideoTracks_: function(stream) {
    var tracks = stream.getVideoTracks();
    for (var t of tracks)
      t.stop();
  },

  /**
   * Discard current photo and return to the live camera stream.
   * @private
   */
  onTapFlipPhoto_: function() {
    this.isFlipped_ = !this.isFlipped_;

    // If there is an existing camera image, (i.e. not a live feed), update the
    // photo to the flipped one.
    if (this.imageUrl_) {
      this.previewImage =
          this.isFlipped_ ? this.flippedImageUrl_ : this.imageUrl_;
      this.fire('phototaken', { photoDataUrl: this.previewImage });
    }

    // TODO(tommycli): Add announce of accessible message for photo flipping.
  },

  /**
   * Discard current photo and return to the live camera stream.
   * @private
   */
  onTapDiscardPhoto_: function() {
    this.cameraTitle = this.i18n('takePhoto');
    this.imageUrl_ = '';
    this.flippedImageUrl_ = '';
    this.previewImage = DEFAULT_IMAGE_URL;
  },

  /**
   * Performs photo capture from the live camera stream. 'phototaken' event
   * will be fired as soon as captured photo is available, with 'dataURL'
   * property containing the photo encoded as a data URL.
   * @private
   */
  onTapTakePhoto_: function() {
    if (!this.cameraOnline_)
      return;
    var canvas = /** @type {HTMLCanvasElement} */(
        document.createElement('canvas'));
    canvas.width = CAPTURE_SIZE.width;
    canvas.height = CAPTURE_SIZE.height;
    this.captureFrame_(
        this.$.cameraVideo,
        /** @type {!CanvasRenderingContext2D} */(canvas.getContext('2d')));

    this.cameraTitle = this.i18n('photoFromCamera');
    this.imageUrl_ = canvas.toDataURL('image/png');
    this.flippedImageUrl_ = this.flipFrame_(canvas);
    this.previewImage =
        this.isFlipped_ ? this.flippedImageUrl_ : this.imageUrl_;

    this.fire('phototaken', { photoDataUrl: this.previewImage });
  },

  /**
   * Captures a single still frame from a <video> element, placing it at the
   * current drawing origin of a canvas context.
   * @param {!HTMLVideoElement} video Video element to capture from.
   * @param {!CanvasRenderingContext2D} ctx Canvas context to draw onto.
   * @private
   */
  captureFrame_: function(video, ctx) {
    var width = video.videoWidth;
    var height = video.videoHeight;
    if (width < CAPTURE_SIZE.width || height < CAPTURE_SIZE.height) {
      console.error('Video capture size too small: ' +
                    width + 'x' + height + '!');
    }
    var src = {};
    if (width / CAPTURE_SIZE.width > height / CAPTURE_SIZE.height) {
      // Full height, crop left/right.
      src.height = height;
      src.width = height * CAPTURE_SIZE.width / CAPTURE_SIZE.height;
    } else {
      // Full width, crop top/bottom.
      src.width = width;
      src.height = width * CAPTURE_SIZE.height / CAPTURE_SIZE.width;
    }
    src.x = (width - src.width) / 2;
    src.y = (height - src.height) / 2;
    ctx.drawImage(video, src.x, src.y, src.width, src.height,
                  0, 0, CAPTURE_SIZE.width, CAPTURE_SIZE.height);
  },

  /**
   * Flips frame horizontally.
   * @param {!(HTMLImageElement|HTMLCanvasElement|HTMLVideoElement)} source
   *     Frame to flip.
   * @return {string} Flipped frame as data URL.
   */
  flipFrame_: function(source) {
    var canvas = document.createElement('canvas');
    canvas.width = CAPTURE_SIZE.width;
    canvas.height = CAPTURE_SIZE.height;
    var ctx = canvas.getContext('2d');
    ctx.translate(CAPTURE_SIZE.width, 0);
    ctx.scale(-1.0, 1.0);
    ctx.drawImage(source, 0, 0);
    return canvas.toDataURL('image/png');
  },

  /**
   * Class of container block. Used by CSS to show, hide, and style controls.
   * @param {boolean} cameraActive
   * @param {string} imageUrl
   * @param {boolean} online
   * @param {boolean} isFlipped
   * @return {string}
   * @private
   */
  containerClass_: function(cameraActive, imageUrl, online, isFlipped) {
    if (!cameraActive)
      return '';

    var classList = [];
    // An empty image URL implies that the user has not already taken a photo,
    // and therefore the camera feed is live.
    if (!imageUrl)
      classList.push('live');
    if (online)
      classList.push('online');
    if (isFlipped)
      classList.push('flip-x');

    return classList.join(' ');
  },
});

})();
