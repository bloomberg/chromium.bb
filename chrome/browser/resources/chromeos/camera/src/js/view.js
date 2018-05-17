// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a named view controller attached to the root element.
 *
 * @param {camera.View.Context} context Context of the app.
 * @param {camera.Router} router View router to switch views.
 * @param {HTMLElement} rootElement Root element containing elements of the
 *     view.
 * @param {string} name View name.
 * @constructor
 */
camera.View = function(context, router, rootElement, name) {
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

  /**
   * @type {HTMLElement}
   * @private
   */
  this.rootElement_ = rootElement;

  /**
   * @type {string}
   * @private
   */
  this.name_ = name;

  /**
   * Remembered tabIndex attribute values of elements on the view.
   * @type {Array.<HTMLElement, string>}
   */
  this.tabIndexes_ = [];
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
  document.body.classList.add(this.name_);
  this.entered_ = true;
  this.onEnter(opt_arguments);
};

/**
 * Leaves the view.
 */
camera.View.prototype.leave = function() {
  this.onLeave();
  this.entered_ = false;
  document.body.classList.remove(this.name_);
};

/**
 * Activates the view. Makes all of the elements on the view focusable.
 */
camera.View.prototype.activate = function() {
  // Restore tabIndex attribute values.
  for (var index = 0; index < this.tabIndexes_.length; index++) {
    var element = this.tabIndexes_[index][0];
    var rememberedTabIndex = this.tabIndexes_[index][1];
    element.tabIndex = rememberedTabIndex;
  }

  this.active_ = true;
  this.onActivate();
};

/**
 * Inactivates the view. Makes all of the elements on the view not focusable.
 */
camera.View.prototype.inactivate = function() {
  this.onInactivate();
  this.active_ = false;

  // Remember tabIndex attribute values, and set them to -1, so the elements
  // are not focusable.
  var childElements = this.rootElement_.querySelectorAll('[tabindex]');
  var elementsArray = Array.prototype.slice.call(childElements);
  if (this.rootElement_.getAttribute('tabindex') !== null)
    elementsArray.push(this.rootElement_);
  this.tabIndexes_ = [];
  for (var index = 0; index < elementsArray.length; index++) {
    var element = elementsArray[index];
    this.tabIndexes_.push([
      element,
      element.tabIndex
    ]);
    element.tabIndex = -1;
  }
};

/**
 * Creates a shared context object for views.
 * @constructor
 */
camera.View.Context = function() {
};

