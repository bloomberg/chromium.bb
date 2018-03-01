// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille commands.
 */

goog.provide('BrailleCommandHandler');

goog.require('BackgroundKeyboardHandler');
goog.require('DesktopAutomationHandler');

goog.scope(function() {
var StateType = chrome.automation.StateType;

/**
 * Customizes ChromeVox commands when issued from a braille display while within
 * editable text.
 * @param {string} command
 * @return {boolean} True if the command should propagate.
 */
BrailleCommandHandler.onEditCommand = function(command) {
  var current = ChromeVoxState.instance.currentRange;
  if (cvox.ChromeVox.isStickyModeOn() || !current || !current.start ||
      !current.start.node || !current.start.node.state[StateType.EDITABLE])
    return true;

  var textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
  if (!textEditHandler)
    return true;

  var isMultiline = AutomationPredicate.multiline(current.start.node);
  switch (command) {
    case 'previousCharacter':
      BackgroundKeyboardHandler.sendKeyPress(37);
      break;
    case 'nextCharacter':
      BackgroundKeyboardHandler.sendKeyPress(39);
      break;
    case 'previousWord':
      BackgroundKeyboardHandler.sendKeyPress(37, {ctrl: true});
      break;
    case 'nextWord':
      BackgroundKeyboardHandler.sendKeyPress(39, {ctrl: true});
      break;
    case 'previousObject':
    case 'previousLine':
      if (!isMultiline || textEditHandler.isSelectionOnFirstLine())
        return true;
      BackgroundKeyboardHandler.sendKeyPress(38);
      break;
    case 'nextObject':
    case 'nextLine':
      if (!isMultiline)
        return true;

      if (textEditHandler.isSelectionOnLastLine()) {
        textEditHandler.moveToAfterEditText();
        return false;
      }

      BackgroundKeyboardHandler.sendKeyPress(40);
      break;
    case 'previousGroup':
      BackgroundKeyboardHandler.sendKeyPress(38, {ctrl: true});
      break;
    case 'nextGroup':
      BackgroundKeyboardHandler.sendKeyPress(40, {ctrl: true});
      break;
    default:
      return true;
  }
  return false;
};

});  //  goog.scope
