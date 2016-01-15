// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A minimal keyboard handler.
 */

goog.provide('KeyboardHandler');

// Implicit dependency on cvox.ChromeVoxKbHandler; cannot include here because
// ChromeVoxKbHandler redefines members.

/**
 * @constructor
 */
KeyboardHandler = function() {
  cvox.ChromeVoxKbHandler.commandHandler = this.handleCommand_.bind(this);
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();
  document.addEventListener('keydown', this.handleKeyDown_.bind(this), false);
};

KeyboardHandler.prototype = {
  /**
   * @param {Event} evt
   * @private
   */
  handleKeyDown_: function(evt) {
    cvox.ExtensionBridge.send({
      'target': 'next',
      'action': 'flushNextUtterance'
    });
    cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  },

  /**
   * @param {string} command
   * @private
   */
  handleCommand_: function(command) {
    cvox.ExtensionBridge.send({
      'target': 'next',
      'action': 'onCommand',
      'command': command
    });
  }
};
