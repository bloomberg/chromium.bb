// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts for the message handler.

goog.provide('__crWeb.message');

goog.require('__crWeb.common');
goog.require('__crWeb.messageDynamic');

/**
 * Namespace for this module.
 */
__gCrWeb.message = {};

/* Beginning of anonymous object. */
(function() {
  /**
   * Object to manage queue of messages waiting to be sent to the main
   * application for immediate processing.
   * @type {Object}
   * @private
   */
  var immediateMessageQueue_ = {
    scheme: 'crwebinvokeimmediate',
    reset: function() {
      immediateMessageQueue_.queue = [];
      // Since the array will be JSON serialized, protect against non-standard
      // custom versions of Array.prototype.toJSON.
      immediateMessageQueue_.queue.toJSON = null;
    }
  };
  immediateMessageQueue_.reset();

  /**
   * Object to manage queue of messages waiting to be sent to the main
   * application for asynchronous processing.
   * @type {Object}
   * @private
   */
  var messageQueue_ = {
    scheme: 'crwebinvoke',
    reset: function() {
      messageQueue_.queue = [];
      // Since the array will be JSON serialized, protect against non-standard
      // custom versions of Array.prototype.toJSON.
      messageQueue_.queue.toJSON = null
    }
  };
  messageQueue_.reset();

  /**
   * Invokes a command immediately on the Objective-C side.
   * An immediate command is a special class of command that must be handled at
   * the earliest opportunity. See the notes in CRWWebController for
   * restrictions and precautions.
   * @param {Object} command The command in a JavaScript object.
   * @private
   */
  __gCrWeb.message.invokeOnHostImmediate = function(command) {
    // If there is no document or body, the command will be silently dropped.
    if (!document || !document.body)
      return;
    immediateMessageQueue_.queue.push(command);
    sendQueue_(immediateMessageQueue_);
  };

  /**
   * Invokes a command on the Objective-C side.
   * @param {Object} command The command in a JavaScript object.
   * @private
   */
  __gCrWeb.message.invokeOnHost = function(command) {
    // Avoid infinite loops in sites that send messages as a side effect
    // of URL verification (e.g., due to logging in an XHR override).
    if (window.__gCrWeb_Verifying) {
      return;
    }
    messageQueue_.queue.push(command);
    sendQueue_(messageQueue_);
  };

  /**
   * Returns the message queue as a string.
   * @return {string} The current message queue as a JSON string.
   */
  __gCrWeb.message.getMessageQueue = function() {
    var messageQueueString = __gCrWeb.common.JSONStringify(messageQueue_.queue);
    messageQueue_.reset()
    return messageQueueString;
  };

  /**
   * Sends both queues if they contain messages.
   */
  __gCrWeb.message.invokeQueues = function() {
    if (immediateMessageQueue_.queue.length > 0)
      sendQueue_(immediateMessageQueue_);
    if (messageQueue_.queue.length > 0)
      sendQueue_(messageQueue_);
  };

  function sendQueue_(queueObject) {
    // Do nothing if windowId has not been set.
    if (!__gCrWeb.windowIdObject || typeof __gCrWeb.windowId != 'string') {
      return;
    }
    // Some pages/plugins implement Object.prototype.toJSON, which can result
    // in serializing messageQueue_ to an invalid format.
    var originalObjectToJSON = Object.prototype.toJSON;
    if (originalObjectToJSON)
      delete Object.prototype.toJSON;
    __gCrWeb.message_dynamic.sendQueue(queueObject);

    if (originalObjectToJSON) {
      // Restore Object.prototype.toJSON to prevent from breaking any
      // functionality on the page that depends on its custom implementation.
      Object.prototype.toJSON = originalObjectToJSON;
    }
  };
}());
