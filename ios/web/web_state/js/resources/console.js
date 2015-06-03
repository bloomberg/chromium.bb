// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts to allow page console.log() etc. output to be seen on the console
// of the host application.

goog.provide('__crWeb.console');

goog.require('__crWeb.message');

/**
 * Namespace for this module.
 */
__gCrWeb.console = {};

/* Beginning of anonymous object. */
(function() {
  function sendConsoleMessage(method, originalArguments) {
    message = Array.prototype.slice.call(originalArguments).join(' ');
    __gCrWeb.message.invokeOnHost({'command': 'console',
                                    'method': method,
                                   'message': message,
                                    'origin': document.location.origin});
  }

  console.log = function() {
    sendConsoleMessage('log', arguments);
  };

  console.debug = function() {
    sendConsoleMessage('debug', arguments);
  };

  console.info = function() {
    sendConsoleMessage('info', arguments);
  };

  console.warn = function() {
    sendConsoleMessage('warn', arguments);
  };

  console.error = function() {
    sendConsoleMessage('error', arguments);
  };
}());
