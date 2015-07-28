// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a compatibility layer for ChromeVox Classic during the
 * transition to ChromeVox Next.
 */

goog.provide('ClassicCompatibility');

goog.require('cvox.ExtensionBridge');
goog.require('cvox.KeyMap');
goog.require('cvox.KeySequence');
goog.require('cvox.KeyUtil');
goog.require('cvox.SimpleKeyEvent');

/**
 * @constructor
 */
var ClassicCompatibility = function() {
  /**
   * @type {!Array<{description: string, name: string, shortcut: string}>}
   * @private
   */
  this.commands_ = [];

  // We grab the list of commands from the manifest because
  // chrome.commands.getAll is buggy.
  /** @type {!Object} */
  var commands = chrome.runtime.getManifest()['commands'];
  for (var key in commands) {
    /** @type {{suggested_key: {chromeos: string}}} */
    var command = commands[key];
    this.commands_.push({name: key, shortcut: command.suggested_key.chromeos});
  }
};

ClassicCompatibility.prototype = {
  /**
   * Processes a ChromeVox Next command.
   * @param {string} command
   * @return {boolean} Whether the command was successfully processed.
   */
  onGotCommand: function(command) {
    var evt = this.buildKeyEvent_(command);
    if (evt) {
      this.simulateKeyDownNext_(evt);
      return true;
    }
    cvox.KeyUtil.sequencing = false;
  },

  /**
   * Processes a ChromeVox Next command while in CLASSIC mode.
   * @param {string} command
   * @return {boolean} Whether the command was successfully processed.
   */
  onGotClassicCommand: function(command) {
    var evt = this.buildKeyEvent_(command);
    if (!evt)
      return false;
    this.simulateKeyDownClassic_(evt);
    return true;
  },

  /**
   * @param {string} command
   * @return {cvox.SimpleKeyEvent?}
   */
  buildKeyEvent_: function(command) {
    var commandInfo = this.commands_.filter(function(c) {
      return c.name == command;
    }.bind(this))[0];
    if (!commandInfo)
      return null;
    var shortcut = commandInfo.shortcut;
    return this.convertCommandShortcutToKeyEvent_(shortcut);
  },

  /**
   * @param {cvox.SimpleKeyEvent} evt
   * @private
   */
  simulateKeyDownNext_: function(evt) {
    var keySequence = cvox.KeyUtil.keyEventToKeySequence(evt);
    var classicCommand =
        cvox.KeyMap.fromCurrentKeyMap().commandForKey(keySequence);
    if (classicCommand) {
      var nextCommand = this.getNextCommand_(classicCommand);
      if (nextCommand)
        global.backgroundObj.onGotCommand(nextCommand, true);
    }
  },

  /**
   * @param {cvox.SimpleKeyEvent} evt
   * @private
   */
  simulateKeyDownClassic_: function(evt) {
    var keySequence = cvox.KeyUtil.keyEventToKeySequence(evt);
    var classicCommand =
        cvox.KeyMap.fromCurrentKeyMap().commandForKey(keySequence);
    if (classicCommand) {
      cvox.ExtensionBridge.send({
        'message': 'USER_COMMAND',
        'command': classicCommand
      });
    }
  },

  /**
   * @param {string} shortcut
   * @return {cvox.SimpleKeyEvent}
   * @private
   */
  convertCommandShortcutToKeyEvent_: function(shortcut) {
    var evt = {};
    shortcut.split('+').forEach(function(token) {
      // Known tokens.
      switch (token) {
        case 'Ctrl':
          evt.ctrlKey = true;
          break;
        case 'Shift':
          evt.shiftKey = true;
          break;
        case 'Alt':
          evt.altKey = true;
          break;
        case 'Search':
          evt.searchKeyHeld = true;
          break;
        case 'Space':
          evt.keyCode = 32;
          break;
        case 'Left':
          evt.keyCode = 37;
          break;
        case 'Up':
          evt.keyCode = 38;
          break;
        case 'Right':
          evt.keyCode = 39;
          break;
        case 'Down':
          evt.keyCode = 40;
          break;
        default:
          evt.keyCode = token.charCodeAt(0);
      }
    });

    return evt;
  },

  /**
   * Maps a Classic command to an approximate equivalent in Next.
   * @param {string} classicCommand
   * @return {string}
   * @private
   */
  getNextCommand_: function(classicCommand) {
    switch (classicCommand) {
      case 'right':
        return 'nextElement';
      case 'forward':
        return 'nextLine';
      case 'left':
        return 'previousElement';
      case 'backward':
        return 'previousLine';
      case 'forceClickOnCurrentItem':
        return 'doDefault';
      case 'readFromHere':
        return 'continuousRead';
      default:
        return classicCommand;
    }
  }
};
