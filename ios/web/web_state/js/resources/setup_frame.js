// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy

/**
 * @fileoverview Notifies the application that this frame has loaded. This file
 * must be included at document end time.
 */

goog.provide('__crWeb.setupFrame');

// Requires __crWeb.common and __crWeb.frameMessaging provided by
// __crWeb.allFramesWebBundle.

/* Beginning of anonymous object. */
(function() {

window.addEventListener('unload', function(event) {
  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',
      __gCrWeb.frameMessaging.getFrameId());
});

/**
 * Listens for messages received by the parent frame to initialize messaging
 * state.
 */
window.addEventListener('message', function(message) {
  var payload = message.data;
  if (payload.hasOwnProperty('type') &&
    payload.type == 'org.chromium.registerForFrameMessaging') {
    __gCrWeb.frameMessaging['getExistingFrames']();
  }
});

// Frame registration must be delayed until Document End script injection time.
// (This file is injected at that time, but the frameMessaging API is defined at
// Document Start time.)
// TODO(crbug.com/873730): Stop exposing registerFrame API.
__gCrWeb.frameMessaging['registerFrame']();

}());  // End of anonymous object
