// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Testing stub for messages.
 */

goog.provide('cvox.TestMsgs');

goog.require('cvox.Msgs');
goog.require('cvox.TestMessages');


/**
 * @constructor
 * @extends {cvox.Msgs}
 */
cvox.TestMsgs = function() {
  cvox.Msgs.call(this);
};
goog.inherits(cvox.TestMsgs, cvox.Msgs);


/**
 * @override
 */
cvox.TestMsgs.prototype.getLocale = function() {
  return 'testing';
};


/**
 * @override
 */
cvox.TestMsgs.prototype.getMsg = function(messageId, opt_subs) {
  if (!messageId) {
    throw Error('Message id required');
  }
  var message = cvox.TestMessages[('chromevox_' + messageId).toUpperCase()];
  if (message == undefined) {
    throw Error('missing-msg: ' + messageId);
  }

  var messageString = message.message;
  if (opt_subs) {
    // Unshift a null to make opt_subs and message.placeholders line up.
    for (var i = 0; i < opt_subs.length; i++) {
      messageString = messageString.replace('$' + (i + 1), opt_subs[i]);
    }
  }
  return messageString;
};
