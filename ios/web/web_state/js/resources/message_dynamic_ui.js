// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts for the message handler for use with UIWebView.

goog.provide('__crWeb.messageDynamic');

goog.require('__crWeb.common');

/**
 * Namespace for this module.
 */
__gCrWeb.message_dynamic = {};

/* Beginning of anonymous object. */
(function() {
  /**
   * Returns true if sending the message queue should be delayed.
   */
  function pageRequiresDelayedSend() {
    // The Apple mobile password recovery page does not interact well with
    // iframes, so on pages with the 'iforgot.apple.com' domain, delay sending
    // the queue.
    return window.location.origin === "https://iforgot.apple.com";
  }

  /**
   * Sends queued commands to the Objective-C side.
   * @param {Object} queueObject Queue object containing messages to send.
   */
  __gCrWeb.message_dynamic.sendQueue = function(queueObject) {
    var send = function() {
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

    // Some pages do not interact well with iframes. On those pages, delay
    // sending the queue.
    if (pageRequiresDelayedSend()) {
      window.requestAnimationFrame(send);
    } else {
      send();
    }
  };
}());
