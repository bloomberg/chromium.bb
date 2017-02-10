// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

goog.provide('cvox.KbExplorer');

goog.require('BrailleCommandHandler');
goog.require('Spannable');
goog.require('cvox.BrailleKeyCommand');
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
  chrome.brailleDisplayPrivate.onKeyEvent.addListener(
      cvox.KbExplorer.onBrailleKeyEvent);
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      cvox.KbExplorer.onAccessibilityGesture);

  window.onbeforeunload = function(evt) {
    backgroundWindow.removeEventListener(
        'keydown', cvox.KbExplorer.onKeyDown, true);
    backgroundWindow.removeEventListener(
        'keyup', cvox.KbExplorer.onKeyUp, true);
    backgroundWindow.removeEventListener(
        'keypress', cvox.KbExplorer.onKeyPress, true);
    chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
        cvox.KbExplorer.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
        cvox.KbExplorer.onAccessibilityGesture);
  };
  if (localStorage['useClassic'] != 'true') {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();
    cvox.ChromeVox.modKeyStr = 'Search';
  } else {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();
    cvox.ChromeVox.modKeyStr = 'Search+Shift';
  }
  cvox.ChromeVoxKbHandler.commandHandler = cvox.KbExplorer.onCommand;
  $('instruction').focus();
};


/**
 * Handles keydown events by speaking the human understandable name of the key.
 * @param {Event} evt key event.
 * @return {boolean} True if the default action should be performed.
 */
cvox.KbExplorer.onKeyDown = function(evt) {
  chrome.extension.getBackgroundPage()['speak'](
      cvox.KeyUtil.getReadableNameForKeyCode(evt.keyCode), false, {pitch: 0});

  // Allow Ctrl+W to be handled.
  if (evt.keyCode == 87 && evt.ctrlKey) {
    return true;
  }

  cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
  return false;
};


/**
 * Handles keyup events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyUp = function(evt) {
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};


/**
 * Handles keypress events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyPress = function(evt) {
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};

/**
 * @param {cvox.BrailleKeyEvent} evt The key event.
 */
cvox.KbExplorer.onBrailleKeyEvent = function(evt) {
  var msgid;
  var msgArgs = [];
  var text;
  switch (evt.command) {
    case cvox.BrailleKeyCommand.PAN_LEFT:
      msgid = 'braille_pan_left';
      break;
    case cvox.BrailleKeyCommand.PAN_RIGHT:
      msgid = 'braille_pan_right';
      break;
    case cvox.BrailleKeyCommand.LINE_UP:
      msgid = 'braille_line_up';
      break;
    case cvox.BrailleKeyCommand.LINE_DOWN:
      msgid = 'braille_line_down';
      break;
    case cvox.BrailleKeyCommand.TOP:
      msgid = 'braille_top';
      break;
    case cvox.BrailleKeyCommand.BOTTOM:
      msgid = 'braille_bottom';
      break;
      break;
    case cvox.BrailleKeyCommand.ROUTING:
    case cvox.BrailleKeyCommand.SECONDARY_ROUTING:
      msgid = 'braille_routing';
      msgArgs.push(/** @type {number} */ (evt.displayPosition + 1));
      break;
    case cvox.BrailleKeyCommand.CHORD:
      if (!evt.brailleDots)
        return;
      var command =
          BrailleCommandHandler.getCommand(evt.brailleDots);
      if (command && cvox.KbExplorer.onCommand(command))
        return;
      // Fall through.
    case cvox.BrailleKeyCommand.DOTS:
      if (!evt.brailleDots)
        return;
      text = BrailleCommandHandler.makeShortcutText(evt.brailleDots);
      break;
    case cvox.BrailleKeyCommand.STANDARD_KEY:
      break;
  }
  if (msgid)
    text = Msgs.getMsg(msgid, msgArgs);
  cvox.KbExplorer.output(text || evt.command);
  cvox.KbExplorer.clearRange();
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the AXGesture enum
 *     defined in ui/accessibility/ax_enums.idl
 */
cvox.KbExplorer.onAccessibilityGesture = function(gesture) {
  // TODO(dmazzoni): implement.
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
    cvox.KbExplorer.output(commandText);
    cvox.KbExplorer.clearRange();
    return true;
  }
};

/**
 * @param {string} text
 * @param {string=} opt_braille If different from text.
 */
cvox.KbExplorer.output = function(text, opt_braille) {
  chrome.extension.getBackgroundPage()['speak'](text);
  chrome.extension.getBackgroundPage().cvox.ChromeVox.braille.write(
      {text: new Spannable(opt_braille || text)});
};

/** Clears ChromeVox range. */
cvox.KbExplorer.clearRange = function() {
  chrome.extension.getBackgroundPage()[
    'ChromeVoxState']['instance']['setCurrentRange'](null);
};
