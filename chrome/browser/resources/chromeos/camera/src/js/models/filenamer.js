// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for models.
 */
cca.models = cca.models || {};

/**
 * Filenamer for single camera session.
 * @param {number=} timestamp Timestamp of camera session.
 * @constructor
 */
cca.models.Filenamer = function(timestamp) {
  /**
   * Timestamp of camera session.
   * @type {number}
   * @private
   */
  this.timestamp_ = timestamp === undefined ? Date.now() : timestamp;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * The prefix of image files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.VIDEO_PREFIX = 'VID_';

/**
 * Creates new filename for video.
 * @return {string} New filename.
 */
cca.models.Filenamer.prototype.newVideoName = function() {
  return cca.models.Filenamer.VIDEO_PREFIX +
      cca.models.Filenamer.timestampToDatetimeName_(this.timestamp_) + '.mkv';
};

/**
 * Creates new filename for image.
 * @return {string} New filename.
 */
cca.models.Filenamer.prototype.newImageName = function() {
  return cca.models.Filenamer.IMAGE_PREFIX +
      cca.models.Filenamer.timestampToDatetimeName_(this.timestamp_) + '.jpg';
};

/**
 * Transforms from capture timestamp to datetime name.
 * @param {number} timestamp Timestamp to be transformed.
 * @return {string} Transformed datetime name.
 * @private
 */
cca.models.Filenamer.timestampToDatetimeName_ = function(timestamp) {
  const pad = (n) => (n < 10 ? '0' : '') + n;
  var date = new Date(timestamp);
  return date.getFullYear() + pad(date.getMonth() + 1) + pad(date.getDate()) +
      '_' + pad(date.getHours()) + pad(date.getMinutes()) +
      pad(date.getSeconds());
};
