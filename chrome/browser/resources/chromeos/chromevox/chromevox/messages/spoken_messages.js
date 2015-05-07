// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Useful abstraction when  speaking messages.
 *
 * Usage:
 * $m('aria_role_link')
 * .andPause()
 * .andMessage('aria_role_forms')
 * .speakFlush();
 *
 */

goog.provide('cvox.SpokenMessages');

goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.SpokenMessage');

/**
 * @type {Array}
 */
cvox.SpokenMessages.messages = [];

/**
 * Speaks the message chain and interrupts any on-going speech.
 */
cvox.SpokenMessages.speakFlush = function() {
  cvox.SpokenMessages.speak(cvox.QueueMode.FLUSH);
};

/**
 * Speaks the message chain after on-going speech finishes.
 */
cvox.SpokenMessages.speakQueued = function() {
  cvox.SpokenMessages.speak(cvox.QueueMode.QUEUE);
};

/**
 * Speak the message chain.
 * @param {cvox.QueueMode} mode The speech queue mode.
 */
cvox.SpokenMessages.speak = function(mode) {
  for (var i = 0; i < cvox.SpokenMessages.messages.length; ++i) {
    var message = cvox.SpokenMessages.messages[i];

    // An invalid message format.
    if (!message || !message.id)
      throw 'Invalid message received.';

    var finalText = cvox.ChromeVox.msgs.getMsg.apply(cvox.ChromeVox.msgs,
                                                     message.id);
    cvox.ChromeVox.tts.speak(finalText, mode,
                             cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);

    // Always queue after the first message.
    mode = cvox.QueueMode.QUEUE;
  }

  cvox.SpokenMessages.messages = [];
};

/**
 * The newest message.
 * @return {cvox.SpokenMessage} The newest (current) message.
 */
cvox.SpokenMessages.currentMessage = function() {
  if (cvox.SpokenMessages.messages.length == 0)
    throw 'Invalid usage of SpokenMessages; start the chain using $m()';
  return cvox.SpokenMessages.messages[cvox.SpokenMessages.messages.length - 1];
};

/**
 * Adds a message.
 * @param {string|Array} messageId The id of the message.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andMessage = function(messageId) {
  var newMessage = new cvox.SpokenMessage();
  newMessage.id = typeof(messageId) == 'string' ? [messageId] : messageId;
  cvox.SpokenMessages.messages.push(newMessage);
  return cvox.SpokenMessages;
};

/**
 * Pauses after the message, with an appropriate marker.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andPause = function() {
  return cvox.SpokenMessages.andMessage('pause');
};

/**
 * Adds a message.
 * @param {string|Array} messageId The id of the message.
 * @return {Object} This object, useful for chaining.
 */
cvox.$m = cvox.SpokenMessages.andMessage;
