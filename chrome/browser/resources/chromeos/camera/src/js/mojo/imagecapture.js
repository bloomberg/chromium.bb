// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for mojo.
 */
cca.mojo = cca.mojo || {};

/**
 * Type definition for cca.mojo.PhotoCapabilities.
 * @typedef {PhotoCapabilities} cca.mojo.PhotoCapabilities
 * @property {Array<string>} [supportedEffects]
 */
cca.mojo.PhotoCapabilities;

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 * @param {MediaStreamTrack} videoTrack A video track whose still images will be
 *     taken.
 * @constructor
 */
cca.mojo.ImageCapture = function(videoTrack) {
  /**
   * @type {ImageCapture}
   * @private
   */
  this.capture_ = new ImageCapture(videoTrack);

  /**
   * @type {cros.mojom.CrosImageCaptureProxy}
   * @private
   */
  this.mojoCapture_ = cros.mojom.CrosImageCapture.getProxy();

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets the photo capabilities with the available options/effects.
 * @return {Promise<cca.mojo.PhotoCapabilities>} Promise for the result.
 */
cca.mojo.ImageCapture.prototype.getPhotoCapabilities = function() {
  return Promise
      .all([
        this.capture_.getPhotoCapabilities(),
        this.mojoCapture_.getSupportedEffects(),
      ])
      .then(([capabilities, effects]) => {
        capabilities.supportedEffects = effects.supportedEffects;
        return capabilities;
      });
};

/**
 * Takes single or multiple photo(s) with the specified settings and effects.
 * The amount of result photo(s) depends on the specified settings and effects,
 * and the first promise in the returned array will always resolve with the
 * unreprocessed photo.
 * @param {?Object} photoSettings Photo settings for ImageCapture's takePhoto().
 * @param {?Array<cros.mojom.Effect>} photoEffects Photo effects to be applied.
 * @return {Array<Promise<Blob>>} Array of promises for the result.
 */
cca.mojo.ImageCapture.prototype.takePhoto = function(
    photoSettings, photoEffects) {
  const takes = [];
  if (photoEffects) {
    photoEffects.forEach((effect) => {
      takes.push((this.mojoCapture_.setReprocessOption(effect))
                     .then(({status, blob}) => {
                       if (status != 0) {
                         throw new Error('Mojo image capture error: ' + status);
                       }
                       const {data, mimeType} = blob;
                       return new Blob(
                           [new Uint8Array(data)], {type: mimeType});
                     }));
    });
  }
  takes.splice(0, 0, this.capture_.takePhoto(photoSettings));
  return takes;
};
