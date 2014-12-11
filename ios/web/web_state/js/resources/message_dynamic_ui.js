// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts for the message handler for use with UIWebView.

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
    // The technique requires the document to be present. If it is not, the
    // messages will be sent once the document object is created.
    if (!document || !document.body) {
      // This happens in rare occasions early in the page cycle. It is
      // possible to create a body element indirectly here, but it has side
      // effects such as blocking page redirection. The safest solution is to
      // try again in 1/10th of a second.
      window.setTimeout(__gCrWeb.message.invokeQueues, 100);
      return;
    }

    var frame = document.createElement('iframe');
    frame.style.display = 'none';
    frame.src = queueObject.scheme + '://' + __gCrWeb.windowId + '#' +
        encodeURIComponent(__gCrWeb.common.JSONStringify(queueObject.queue));
    queueObject.reset();
    document.body.appendChild(frame);
    document.body.removeChild(frame);
  };
}
