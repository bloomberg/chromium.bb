// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used for secure communication with frames.
 */

goog.provide('__crWeb.frameMessaging');

goog.require('__crWeb.base');

/**
 * Namespace for this module.
 */
__gCrWeb.frameMessaging = {};

// Store message namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['frameMessaging'] = __gCrWeb.frameMessaging;

/** Beginning of anonymous object */
(function() {

/**
* Unique identifier for this frame.
* @type {string}
* @private
*/
var frameId_ = undefined;

/**
 * Returns the frameId associated with this frame. A new value will be created
 * the first time this function is called for this frame.
 * @return {string} A string representing a unique identifier for this frame.
 */
__gCrWeb.frameMessaging['getFrameId'] = function() {
  if (!frameId_) {
    frameId_ = "";
    // Generate 128 bit unique identifier.
    var components = new Uint32Array(4);
    window.crypto.getRandomValues(components);
    for (var i = 0; i < components.length; i++) {
      // Convert value to base16 string and append to the |frameId_|.
      frameId_ += components[i].toString(16);
    }
  }
  return frameId_;
};

}());  // End of anonymous object
