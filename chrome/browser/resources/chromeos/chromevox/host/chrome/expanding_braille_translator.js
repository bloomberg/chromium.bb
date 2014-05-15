// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translates text to braille, optionally with some parts
 * uncontracted.
 */

goog.provide('cvox.ExpandingBrailleTranslator');

goog.require('cvox.BrailleUtil');
goog.require('cvox.LibLouis');
goog.require('cvox.Spannable');


/**
 * A wrapper around one or two braille translators that uses contracted
 * braille or not based on the selection start- and end-points (if any) in the
 * translated text.  If only one translator is provided, then that translator
 * is used for all text regardless of selection.  If two translators
 * are provided, then the uncontracted translator is used for some text
 * around the selection end-points and the contracted translator is used
 * for all other text.  When determining what text to use uncontracted
 * translation for around a position, a region surrounding that position
 * containing either only whitespace characters or only non-whitespace
 * characters is used.
 * @param {!cvox.LibLouis.Translator} defaultTranslator The translator for all
 *     text when the uncontracted translator is not used.
 * @param {cvox.LibLouis.Translator=} opt_uncontractedTranslator
 *     Translator to use for uncontracted braille translation.
 * @constructor
 */
cvox.ExpandingBrailleTranslator =
    function(defaultTranslator, opt_uncontractedTranslator) {
  /**
   * @type {!cvox.LibLouis.Translator}
   * @private
   */
  this.defaultTranslator_ = defaultTranslator;
  /**
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.uncontractedTranslator_ = opt_uncontractedTranslator || null;
};


/**
 * Translates text to braille using the translator(s) provided to the
 * constructor.  See {@code cvox.LibLouis.Translator} for further details.
 * @param {!cvox.Spannable} text Text to translate.
 * @param {function(!ArrayBuffer, !Array.<number>, !Array.<number>)}
 *     callback Called when the translation is done.  It takes resulting
 *         braille cells and positional mappings as parameters.
 */
cvox.ExpandingBrailleTranslator.prototype.translate =
    function(text, callback) {
  var expandRanges = this.findExpandRanges_(text);
  if (expandRanges.length == 0) {
    this.defaultTranslator_.translate(
        text.toString(),
        cvox.ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
            text.getLength(), callback));
    return;
  }

  var chunks = [];
  function addChunk(translator, start, end) {
    chunks.push({translator: translator, start: start, end: end});
  }
  var lastEnd = 0;
  for (var i = 0; i < expandRanges.length; ++i) {
    var range = expandRanges[i];
    if (lastEnd < range.start) {
      addChunk(this.defaultTranslator_, lastEnd, range.start);
    }
    addChunk(this.uncontractedTranslator_, range.start, range.end);
    lastEnd = range.end;
  }
  if (lastEnd < text.getLength()) {
    addChunk(this.defaultTranslator_, lastEnd, text.getLength());
  }

  var numPendingCallbacks = chunks.length;

  function chunkTranslated(chunk, cells, textToBraille, brailleToText) {
    chunk.cells = cells;
    chunk.textToBraille = textToBraille;
    chunk.brailleToText = brailleToText;
    if (--numPendingCallbacks <= 0) {
      finish();
    }
  }

  function finish() {
    var totalCells = chunks.reduce(
        function(accum, chunk) { return accum + chunk.cells.byteLength}, 0);
    var cells = new Uint8Array(totalCells);
    var cellPos = 0;
    var textToBraille = [];
    var brailleToText = [];
    function appendAdjusted(array, toAppend, adjustment) {
      array.push.apply(array, toAppend.map(
          function(elem) { return adjustment + elem; }
          ));
    }
    for (var i = 0, chunk; chunk = chunks[i]; ++i) {
      cells.set(new Uint8Array(chunk.cells), cellPos);
      appendAdjusted(textToBraille, chunk.textToBraille, cellPos);
      appendAdjusted(brailleToText, chunk.brailleToText, chunk.start);
      cellPos += chunk.cells.byteLength;
    }
    callback(cells.buffer, textToBraille, brailleToText);
  }

  for (var i = 0, chunk; chunk = chunks[i]; ++i) {
    chunk.translator.translate(
        text.toString().substring(chunk.start, chunk.end),
        cvox.ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
            chunk.end - chunk.start, goog.partial(chunkTranslated, chunk)));
  }
};


/**
 * Expands a position to a range that covers the consecutive range of
 * either whitespace or non whitespace characters around it.
 * @param {string} str Text to look in.
 * @param {number} pos Position to start looking at.
 * @return {!cvox.ExpandingBrailleTranslator.Range_}
 * @private
 */
cvox.ExpandingBrailleTranslator.rangeForPosition_ = function(str, pos) {
  if (pos < 0 || pos >= str.length) {
    throw Error('Position out of range looking for braille expansion range');
  }
  // Find the last chunk of either whitespace or non-whitespace before and
  // including pos.
  var start = str.substring(0, pos + 1).search(/(\s+|\S+)$/);
  // Find the characters to include after pos, starting at pos so that
  // they are the same kind (either whitespace or not) as the
  // characters starting at start.
  var end = pos + /^(\s+|\S+)/.exec(str.substring(pos))[0].length;
  return {start: start, end: end};
};


/**
 * Finds the ranges in which contracted braille should not be used.
 * @param {!cvox.Spannable} text Text to find expansion ranges in.
 * @return {!Array.<cvox.ExpandingBrailleTranslator.Range_>}
 * @private
 */
cvox.ExpandingBrailleTranslator.prototype.findExpandRanges_ = function(text) {
  var result = [];
  if (this.uncontractedTranslator_) {
    var selection = text.getSpanInstanceOf(cvox.BrailleUtil.ValueSelectionSpan);
    if (selection) {
      var selectionStart = text.getSpanStart(selection);
      var selectionEnd = text.getSpanEnd(selection);
      if (selectionStart < text.getLength()) {
        var expandPositions = [selectionStart];
        // Include the selection end if the length of the selection is greater
        // than one (otherwise this position would be redundant).
        if (selectionEnd > selectionStart + 1) {
          // Look at the last actual character of the selection, not the
          // character at the (exclusive) end position.
          expandPositions.push(selectionEnd - 1);
        }

        var lastRange = null;
        for (var i = 0; i < expandPositions.length; ++i) {
          var range = cvox.ExpandingBrailleTranslator.rangeForPosition_(
              text.toString(), expandPositions[i]);
          if (lastRange && lastRange.end >= range.start) {
            lastRange.end = range.end;
          } else {
            result.push(range);
            lastRange = range;
          }
        }
      }
    }
  }

  return result;
};


/**
 * Adapts {@code callback} to accept null arguments and treat them as if the
 * translation result is empty.
 * @param {number} inputLength Length of the input to the translation.
 *     Used for populating {@code textToBraille} if null.
 * @param {function(!ArrayBuffer, !Array.<number>, !Array.<number>)} callback
 * @return {function(ArrayBuffer, Array.<number>, Array.<number>)}
 * @private
 */
cvox.ExpandingBrailleTranslator.nullParamsToEmptyAdapter_ =
    function(inputLength, callback) {
  return function(cells, textToBraille, brailleToText) {
    if (!textToBraille) {
      textToBraille = new Array(inputLength);
      for (var i = 0; i < inputLength; ++i) {
        textToBraille[i] = 0;
      }
    }
    callback(cells || new ArrayBuffer(0),
             textToBraille,
             brailleToText || []);
  };
};


/**
 * A character range with inclusive start and exclusive end positions.
 * @typedef {{start: number, end: number}}
 * @private
 */
cvox.ExpandingBrailleTranslator.Range_;
