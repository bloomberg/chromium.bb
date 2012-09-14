// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  The tumbler Application object.  This object instantiates a
 * Trackball object and connects it to the element named |tumbler_content|.
 * It also conditionally embeds a debuggable module or a release module into
 * the |tumbler_content| element.
 */

// Requires tumbler
// Requires tumbler.Dragger
// Requires tumbler.Trackball

var tumbler = tumbler || {};

/**
 * Constructor for the Application class.  Use the run() method to populate
 * the object with controllers and wire up the events.
 * @constructor
 */
tumbler.Application = function() {
  /**
   * The native module for the application.  This refers to the module loaded
   * via the <embed> tag.
   * @type {Element}
   * @private
   */
  this.module_ = null;

  /**
   * The trackball object.
   * @type {tumbler.Trackball}
   * @private
   */
  this.trackball_ = null;

  /**
   * The mouse-drag event object.
   * @type {tumbler.Dragger}
   * @private
   */
  this.dragger_ = null;
}

/**
 * Called by common.js when the NaCl module has been loaded.
 */
function moduleDidLoad() {
  tumbler.application = new tumbler.Application();
  tumbler.application.moduleDidLoad();
}

/**
 * Called by the module loading function once the module has been loaded.
 */
tumbler.Application.prototype.moduleDidLoad = function() {
  this.module_ = document.getElementById('nacl_module');

  /**
   * Set the camera orientation property on the NaCl module.
   * @param {Array.<number>} orientation A 4-element array representing the
   *     camera orientation as a quaternion.
   */
  this.module_.setCameraOrientation = function(orientation) {
      var methodString = 'setCameraOrientation ' +
                         'orientation:' +
                         JSON.stringify(orientation);
      this.postMessage(methodString);
  }

  this.trackball_ = new tumbler.Trackball();
  this.dragger_ = new tumbler.Dragger(this.module_);
  this.dragger_.addDragListener(this.trackball_);
}

/**
 * Asserts that cond is true; issues an alert and throws an Error otherwise.
 * @param {bool} cond The condition.
 * @param {String} message The error message issued if cond is false.
 */
tumbler.Application.prototype.assert = function(cond, message) {
  if (!cond) {
    message = "Assertion failed: " + message;
    alert(message);
    throw new Error(message);
  }
}
