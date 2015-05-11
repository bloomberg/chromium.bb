// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a compatibility layer for ChromeVox Classic during the
 * transition to ChromeVox Next.
 */

goog.provide('ClassicCompatibility');

goog.require('cvox.KeyMap');
goog.require('cvox.KeySequence');
goog.require('cvox.KeyUtil');
goog.require('cvox.SimpleKeyEvent');

/**
 * @param {boolean=} opt_active Whether compatibility is currently active.
 * @constructor
 */
var ClassicCompatibility = function(opt_active) {
  /** @type {boolean} */
  this.active = !!opt_active;

  /**
   * @type {!Array<{description: string, name: string, shortcut: string}>}
   * @private
   */
  this.commands_ = [];

  /** @type {!cvox.KeyMap} */
  this.keyMap = cvox.KeyMap.fromCurrentKeyMap();

  chrome.commands.getAll(function(commands) {
    this.commands_ = commands;
  }.bind(this));
};

ClassicCompatibility.prototype = {
  /**
   * Processes a ChromeVox Next command.
   * @param {string} command
   * @return {boolean} Whether the command was successfully processed.
   */
  onGotCommand: function(command) {
    if (!this.active)
      return false;

    var commandInfo = this.commands_.filter(function(c) {
      return c.name == command;
    }.bind(this))[0];
    if (!commandInfo)
      return false;
    var shortcut = commandInfo.shortcut;
    var evt = this.convertCommandShortcutToKeyEvent_(shortcut);
    this.simulateKeyDown_(evt);
    return true;
  },

  /**
   * @param {cvox.SimpleKeyEvent} evt
   * @private
   */
  simulateKeyDown_: function(evt) {
    var keySequence = cvox.KeyUtil.keyEventToKeySequence(evt);
    var classicCommand = this.keyMap.commandForKey(keySequence);
    if (classicCommand) {
      var nextCommand = this.getNextCommand_(classicCommand);
      if (nextCommand)
        global.backgroundObj.onGotCommand(nextCommand, true);
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
        case 'Left Arrow':
          evt.keyCode = 37;
          break;
        case 'Up Arrow':
          evt.keyCode = 38;
          break;
        case 'Right Arrow':
          evt.keyCode = 39;
          break;
        case 'Down Arrow':
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
      default:
        return classicCommand;
    }
  }
};
