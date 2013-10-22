// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a view controller.
 *
 * @param {camera.View.Context} context Context of the app.
 * @param {camera.Router} router View router to switch views.
 * @constructor
 */
camera.View = function(context, router) {
  /**
   * @type {boolean}
   * @private
   */
  this.active_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.entered_ = false;

  /**
   * @type {camera.View.Context}
   * @private
   */
  this.context_ = context;

  /**
   * @type {camera.Router}
   * @private
   */
  this.router_ = router;
};

camera.View.prototype = {
  get entered() {
    return this.entered_;
  },
  get active() {
    return this.active_;
  },
  get context() {
    return this.context_;
  },
  get router() {
    return this.router_;
  }
};

/**
 * Processes key pressed events.
 * @param {Event} event Key event.
 */
camera.View.prototype.onKeyPressed = function(event) {
};

/**
 * Processes resizing events.
 */
camera.View.prototype.onResize = function() {
};

/**
 * Handles enters the view.
 * @param {Object=} opt_arguments Optional arguments.
 */
camera.View.prototype.onEnter = function(opt_arguments) {
};

/**
 * Handles leaving the view.
 */
camera.View.prototype.onLeave = function() {
};

/**
 * Handles activating the view.
 */
camera.View.prototype.onActivate = function() {
};

/**
 * Handles inactivating the view.
 */
camera.View.prototype.onInactivate = function() {
};

/**
 * Initializes the view. Call the callback on completion.
 * @param {function()} callback Completion callback.
 */
camera.View.prototype.initialize = function(callback) {
  callback();
};

/**
 * Enters the view.
 * @param {Object=} opt_arguments Optional arguments.
 */
camera.View.prototype.enter = function(opt_arguments) {
  this.entered_ = true;
  this.onEnter(opt_arguments);
};

/**
 * Leaves the view.
 */
camera.View.prototype.leave = function() {
  this.entered_ = false;
  this.onLeave();
};

/**
 * Activates the view.
 */
camera.View.prototype.activate = function() {
  this.active_ = true;
  this.onActivate();
};

/**
 * Inactivates the view.
 */
camera.View.prototype.inactivate = function() {
  this.active_ = false;
  this.onInactivate();
};

/**
 * Creates a shared context object for views.
 * @constructor
 */
camera.View.Context = function() {
};

