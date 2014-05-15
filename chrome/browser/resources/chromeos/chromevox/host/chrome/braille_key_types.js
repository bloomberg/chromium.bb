// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Braille command definitions.
 * These types are adapted from Chrome's private braille API.
 * They can be found in the Chrome source repo at:
 * src/chrome/common/extensions/api/braille_display_private.idl
 * We define them here since they don't actually exist as bindings under
 * chrome.brailleDisplayPrivate.*.
 */

goog.provide('cvox.BrailleDisplayState');
goog.provide('cvox.BrailleKeyCommand');
goog.provide('cvox.BrailleKeyEvent');

goog.require('cvox.ChromeVox');


/**
 * The set of commands sent from a braille display.
 * @enum {string}
 */
cvox.BrailleKeyCommand = {
  PAN_LEFT: 'pan_left',
  PAN_RIGHT: 'pan_right',
  LINE_UP: 'line_up',
  LINE_DOWN: 'line_down',
  TOP: 'top',
  BOTTOM: 'bottom',
  ROUTING: 'routing',
  SECONDARY_ROUTING: 'secondary_routing',
  DOTS: 'dots',
  STANDARD_KEY: 'standard_key'
};


/**
 * Represents a key event from a braille display.
 *
 * @typedef {{command: cvox.BrailleKeyCommand,
 *            displayPosition: (undefined|number),
 *            brailleDots: (undefined|number)
 *          }}
 *  command The name of the command.
 *  displayPosition The 0-based position relative to the start of the currently
 *                  displayed text.  Used for commands that involve routing
 *                  keys or similar.  The position is given in characters,
 *                  not braille cells.
 *  dots Dots that were pressed for braille input commands.  Bit mask where
 *       bit 0 represents dot 1 etc.
 */
cvox.BrailleKeyEvent = {};


/**
 * The state of a braille display as represented in the
 * chrome.brailleDisplayPrivate API.
 * @typedef {{available: boolean, textCellCount: (number|undefined)}}
 */
cvox.BrailleDisplayState;
