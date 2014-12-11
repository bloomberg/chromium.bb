// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts for the message handler for use with WKWebView.

/**
 * Namespace for this module.
 */
__gCrWeb.message_dynamic = {};

/* Beginning of anonymous object. */
new function() {
  /**
   * Sends queued commands to the Objective-C side.
   * @param {Object} queueObject Queue object containing messages to send.
   */
  __gCrWeb.message_dynamic.sendQueue = function(queueObject) {
    queueObject.queue.forEach(function(command) {
        var stringifiedMessage = __gCrWeb.common.JSONStringify({
            "crwCommand": command,
            "crwWindowId": __gCrWeb.windowId
        });
        window.webkit.messageHandlers[queueObject.scheme].postMessage(
            stringifiedMessage);
    });
    queueObject.reset();
  };
}
