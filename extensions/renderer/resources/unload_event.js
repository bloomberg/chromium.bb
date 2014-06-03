// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Special unload event. We don't use the DOM unload because that slows down
// tab shutdown. On the other hand, onUnload might not always fire, since
// Chrome will terminate renderers on shutdown (SuddenTermination).

// Implement a primitive subset of the Event interface as needed, since if this
// was to use the real event object there would be a circular dependency.
var listeners = [];

exports.addListener = function(listener) {
  $Array.push(listeners, listener);
};

exports.removeListener = function(listener) {
  for (var i = 0; i < listeners.length; ++i) {
    if (listeners[i] == listener) {
      $Array.splice(listeners, i, 1);
      return;
    }
  }
};

exports.wasDispatched = false;

// dispatch() is called from C++.
exports.dispatch = function() {
  exports.wasDispatched = true;
  for (var i = 0; i < listeners.length; ++i)
    listeners[i]();
};
