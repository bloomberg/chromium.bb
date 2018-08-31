// Copyright 2018 The Chhomium OS Authors. All rights reserved.
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
 * Creates a controller for the record-time of Camera view.
 * @constructor
 */
camera.views.camera.RecordTime = function() {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.recordTime_ = document.querySelector('#record-time');

  /**
   * Timeout to count every tick of elapsed recording time.
   * @type {?number}
   * @private
   */
  this.tickTimeout_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Updates UI by the elapsed recording time.
 * @param {number} time Time in seconds.
 * @private
 */
camera.views.camera.RecordTime.prototype.update_ = function(time) {
  // Format time into HH:MM:SS or MM:SS.
  var pad = (n) => {
    return (n < 10 ? '0' : '') + n;
  };
  var hh = '';
  if (time >= 3600) {
    hh = pad(Math.floor(time / 3600)) + ':';
  }
  var mm = pad(Math.floor(time / 60) % 60) + ':';
  document.querySelector('#record-time-msg').textContent =
      hh + mm + pad(time % 60);
};

/**
 * Starts to count and show the elapsed recording time.
 */
camera.views.camera.RecordTime.prototype.start = function() {
  this.update_(0);
  this.recordTime_.classList.add('visible');

  var ticks = 0;
  this.tickTimeout_ = setInterval(() => {
    ticks++;
    this.update_(ticks);
  }, 1000);
};

/**
 * Stops counting and showing the elapsed recording time.
 */
camera.views.camera.RecordTime.prototype.stop = function() {
  if (this.tickTimeout_) {
    clearInterval(this.tickTimeout_);
    this.tickTimeout_ = null;
  }
  this.recordTime_.classList.remove('visible');
  this.update_(0);
};
