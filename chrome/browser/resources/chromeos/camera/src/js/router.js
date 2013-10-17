// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates the Router object, used to navigate between views.
 *
 * @param {function(camera.View}) callback Callback to be called, when a view
 *     switch is requested. This callback should perform the switch.
 * @constructor
 */
camera.Router = function(callback) {
  /**
   * @param function()
   * @private
   */
  this.callback_ = callback;

  // End of properties. Freeze the object.
  Object.freeze(this);
};

/**
 * View identifiers.
 * @enum {number}
 */
camera.Router.ViewIdentifier = {
  CAMERA: 0,
  GALLERY: 1,
  BROWSER: 2
};

/**
 * Switches to the specified view.
 * @param {camera.Router.ViewIdentifier} viewIdentifier View
 *     identifier.
 */
camera.Router.prototype.switchView = function(viewIdentifier) {
  this.callback_(viewIdentifier);
};

