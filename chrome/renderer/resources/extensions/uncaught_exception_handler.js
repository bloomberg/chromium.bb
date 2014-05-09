// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles uncaught exceptions thrown by extensions. By default this is to
// log an error message, but tests may override this behaviour.

var handler = function(message, e) {
  console.error(message);
};

// |message| The message associated with the error.
// |e|       The object that was thrown.
exports.handle = function(message, e) {
  handler(message, e);
};

// |newHandler| A function which matches |exports.handle|.
exports.setHandler = function(newHandler) {
  handler = newHandler;
};
