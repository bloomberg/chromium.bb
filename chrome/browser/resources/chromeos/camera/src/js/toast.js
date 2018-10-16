// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a toast controller for all views.
 * @constructor
 */
camera.Toast = function() {
  /**
   * Timeout for hiding the toast message after some delay.
   * @type {?number}
   * @private
   */
  this.hideTimeout_ = null;

  /**
   * Toast transition wrapper. Shows or hides the toast with the passed message.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.effect_ = new camera.util.StyleEffect((args, callback) => {
    var element = document.querySelector('#toast');
    // Hide the message if visible.
    if (!args.visible && element.classList.contains('visible')) {
      element.classList.remove('visible');
      camera.util.waitForTransitionCompletion(element, 500, callback);
    } else if (args.visible) {
      // If showing requested, then show.
      element.textContent = args.message;
      element.classList.add('visible');
      camera.util.waitForTransitionCompletion(element, 500, callback);
    } else {
      callback();
    }
  });

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Shows a message in the toast.
 * @param {string} message Message to be shown.
 */
camera.Toast.prototype.showMessage = function(message) {
  var cancelHide = () => {
    if (this.hideTimeout_) {
      clearTimeout(this.hideTimeout_);
      this.hideTimeout_ = null;
    }
  };
  // If running, then reinvoke recursively after closing the toast message.
  if (this.effect_.animating || this.hideTimeout_) {
    cancelHide();
    this.effect_.invoke({
      visible: false
    }, this.showMessage.bind(this, message));
    return;
  }
  // Cancel any pending hide-timeout before starting a new timeout.
  cancelHide();
  this.hideTimeout_ = setTimeout(() => {
    this.effect_.invoke({
      visible: false
    }, () => {});
    this.hideTimeout_ = null;
  }, 2000);
  // Show the toast message.
  this.effect_.invoke({
    visible: true,
    message: message
  }, () => {});
};
