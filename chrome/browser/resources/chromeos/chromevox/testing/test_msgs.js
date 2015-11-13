// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Testing stub for messages.
 */

goog.provide('TestMsgs');

goog.require('Msgs');
goog.require('cvox.TestMessages');

/**
 * @constructor
 */
TestMsgs = function() {};

/**
 * @type {Object<string>}
 */
TestMsgs.Untranslated = Msgs.Untranslated;

/**
 * @return {string} The locale.
 */
TestMsgs.getLocale = function() {
  return 'testing';
};

/**
 * @param {string} messageId
 * @param {Array<string>=} opt_subs
 * @return {string}
 */
TestMsgs.getMsg = function(messageId, opt_subs) {
  if (!messageId) {
    throw Error('Message id required');
  }
  var message = TestMsgs.Untranslated[messageId.toUpperCase()];
  if (message !== undefined)
    return message;
  message = cvox.TestMessages[('chromevox_' + messageId).toUpperCase()];
  if (message === undefined) {
    throw Error('missing-msg: ' + messageId);
  }

  var messageString = message.message;
  var placeholders = message.placeholders;
  if (placeholders) {
    for (name in placeholders) {
      messageString = messageString.replace(
          '$' + name + '$',
          placeholders[name].content);
    }
  }
  if (opt_subs) {
    // Unshift a null to make opt_subs and message.placeholders line up.
    for (var i = 0; i < opt_subs.length; i++) {
      messageString = messageString.replace('$' + (i + 1), opt_subs[i]);
    }
  }
  return messageString;
};

/**
 * @param {number} num
 * @return {string}
 */
TestMsgs.getNumber = Msgs.getNumber;
