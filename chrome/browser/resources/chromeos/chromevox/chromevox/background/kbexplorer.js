// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

goog.provide('cvox.KbExplorer');

goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.CommandStore');
goog.require('cvox.KeyMap');
goog.require('cvox.KeyUtil');


/**
 * Class to manage the keyboard explorer.
 * @constructor
 */
cvox.KbExplorer = function() { };


/**
 * Initialize keyboard explorer.
 */
cvox.KbExplorer.init = function() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  backgroundWindow.addEventListener(
      'keydown', cvox.KbExplorer.onKeyDown, true);
  backgroundWindow.addEventListener('keyup', cvox.KbExplorer.onKeyUp, true);
  backgroundWindow.addEventListener(
      'keypress', cvox.KbExplorer.onKeyPress, true);

  window.onbeforeunload = function(evt) {
    backgroundWindow.removeEventListener(
        'keydown', cvox.KbExplorer.onKeyDown, true);
    backgroundWindow.removeEventListener(
        'keyup', cvox.KbExplorer.onKeyUp, true);
    backgroundWindow.removeEventListener(
        'keypress', cvox.KbExplorer.onKeyPress, true);
  };
  if (localStorage['useNext'] == 'true') {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();
    cvox.ChromeVox.modKeyStr = 'Search';
  } else {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();
    cvox.ChromeVox.modKeyStr = 'Search+Shift';
  }
  cvox.ChromeVoxKbHandler.commandHandler = cvox.KbExplorer.onCommand;
};


/**
 * Handles keydown events by speaking the human understandable name of the key.
 * @param {Event} evt key event.
 * @return {boolean} True if the default action should be performed.
 */
cvox.KbExplorer.onKeyDown = function(evt) {
  chrome.extension.getBackgroundPage()['speak'](
      cvox.KeyUtil.getReadableNameForKeyCode(evt.keyCode), false, {});

  // Allow Ctrl+W to be handled.
  if (evt.keyCode == 87 && evt.ctrlKey) {
    return true;
  }

  cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt);

  evt.preventDefault();
  evt.stopPropagation();
  return false;
};


/**
 * Handles keyup events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyUp = function(evt) {
  evt.preventDefault();
  evt.stopPropagation();
};


/**
 * Handles keypress events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyPress = function(evt) {
  evt.preventDefault();
  evt.stopPropagation();
};

/**
 * Queues up command description.
 * @param {string} command
 * @return {boolean|undefined} True if command existed and was handled.
 */
cvox.KbExplorer.onCommand = function(command) {
  var msg = cvox.CommandStore.messageForCommand(command);
  if (msg) {
    var commandText = Msgs.getMsg(msg);
    chrome.extension.getBackgroundPage()['speak'](commandText);
    return true;
  }
};
