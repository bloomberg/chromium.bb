// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille commands.
 */

goog.provide('BrailleCommandHandler');

goog.require('ChromeVoxState');
goog.require('CommandHandler');


goog.scope(function() {
/**
 * Maps a dot pattern to a command.
 * @type {!Object<number, string>}
 */
BrailleCommandHandler.DOT_PATTERN_TO_COMMAND = {
};

/**
 * Makes a dot pattern given a list of dots numbered from 1 to 8 arranged in a
 * braille cell (a 2 x 4 dot grid).
 * @param {Array<number>} dots The dots to be set in the returned pattern.
 * @return {number}
 */
BrailleCommandHandler.makeDotPattern = function(dots) {
  return dots.reduce(function(p, c) {
    return p | (1 << c - 1);
  }, 0);
};

/**
 * Perform a braille command based on a dot pattern from a chord.
 * @param {number} dots Braille dot pattern
 */
BrailleCommandHandler.onBrailleCommand = function(dots) {
  var command = BrailleCommandHandler.DOT_PATTERN_TO_COMMAND[dots];
  if (command)
    CommandHandler.onCommand(command);
};

/**
 * @private
 */
BrailleCommandHandler.init_ = function() {
  var map = function(dots, command) {
    BrailleCommandHandler.DOT_PATTERN_TO_COMMAND[
      BrailleCommandHandler.makeDotPattern(dots)] = command;
  };

  map([2, 3], 'previousGroup');
  map([5, 6], 'nextGroup');
  map([1], 'previousObject');
  map([4], 'nextObject');
  map([2], 'previousWord');
  map([5], 'nextWord');
  map([3], 'previousCharacter');
  map([6], 'nextCharacter');
  map([1, 2, 3], 'jumpToTop');
  map([4, 5, 6], 'jumpToBottom');

  map([1, 4], 'fullyDescribe');
  map([1, 3, 4], 'contextMenu');
  map([1, 2, 3, 5], 'readFromHere');
  map([2, 3, 4], 'toggleSelection');

  // Forward jump.
  map([1, 2], 'nextButton');
  map([1, 5], 'nextEditText');
  map([1, 2, 4], 'nextFormField');
  map([1, 2, 5], 'nextHeading');
  map([4, 5], 'nextLink');
  map([2, 3, 4, 5], 'nextTable');

  // Backward jump.
  map([1, 2, 7], 'previousButton');
  map([1, 5, 7], 'previousEditText');
  map([1, 2, 4, 7], 'previousFormField');
  map([1, 2, 5, 7], 'previousHeading');
  map([4, 5, 7], 'previousLink');
  map([2, 3, 4, 5, 7], 'previousTable');

  map([8], 'forceClickOnCurrentItem');
  map([3, 4], 'toggleSearchWidget');

  // Question.
  map([1, 4, 5, 6], 'toggleKeyboardHelp');
};

BrailleCommandHandler.init_();

}); //  goog.scope
