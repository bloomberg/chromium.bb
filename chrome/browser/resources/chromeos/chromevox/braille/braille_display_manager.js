// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Puts text on a braille display.
 *
 */

goog.provide('cvox.BrailleDisplayManager');

goog.require('cvox.BrailleCaptionsBackground');
goog.require('cvox.BrailleDisplayState');
goog.require('cvox.ExpandingBrailleTranslator');
goog.require('cvox.LibLouis');
goog.require('cvox.NavBraille');
goog.require('cvox.PanStrategy');


/**
 * @param {!cvox.BrailleTranslatorManager} translatorManager Keeps track
 *     of the current translator to use.
 * @constructor
 */
cvox.BrailleDisplayManager = function(translatorManager) {
  /**
   * @type {!cvox.BrailleTranslatorManager}
   * @private
   */
  this.translatorManager_ = translatorManager;
  /**
   * @type {!cvox.NavBraille}
   * @private
   */
  this.content_ = new cvox.NavBraille({});
  /**
   * @type {!cvox.ExpandingBrailleTranslator.ExpansionType} valueExpansion
   * @private
   */
  this.expansionType_ =
      cvox.ExpandingBrailleTranslator.ExpansionType.SELECTION;
  /**
   * @type {cvox.PanStrategy}
   * @private
   */
  this.panStrategy_ = new cvox.PanStrategy();
  /**
   * @type {function(!cvox.BrailleKeyEvent, !cvox.NavBraille)}
   * @private
   */
  this.commandListener_ = function() {};
  /**
   * Current display state to show in the Virtual Braille Captions display.
   * This is different from realDisplayState_ if the braille captions feature
   * is enabled and there is no hardware display connected.  Otherwise, it is
   * the same object as realDisplayState_.
   * @type {!cvox.BrailleDisplayState}
   * @private
   */
  this.displayState_ = {available: false, textRowCount: undefined,
      textColumnCount: undefined};
  /**
   * State reported from the chrome api, reflecting a real hardware
   * display.
   * @type {!cvox.BrailleDisplayState}
   * @private
   */
  this.realDisplayState_ = this.displayState_;

  translatorManager.addChangeListener(function() {
    this.translateContent_(this.content_, this.expansionType_);
  }.bind(this));

  chrome.storage.onChanged.addListener(function(changes, area) {
    if (area == 'local' && 'brailleWordWrap' in changes) {
      this.updatePanStrategy_(changes.brailleWordWrap.newValue);
    }
    if (area == 'local' && ('virtualBrailleRows' in changes ||
          'virtualBrailleColumns' in changes)) {
      this.onCaptionsStateChanged_();
    }
  }.bind(this));
  chrome.storage.local.get({brailleWordWrap: true}, function(items) {
    this.updatePanStrategy_(items.brailleWordWrap);
  }.bind(this));

  cvox.BrailleCaptionsBackground.init(goog.bind(
      this.onCaptionsStateChanged_, this));
  if (goog.isDef(chrome.brailleDisplayPrivate)) {
    var onDisplayStateChanged = goog.bind(this.refreshDisplayState_, this);
    chrome.brailleDisplayPrivate.getDisplayState(onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
        onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        goog.bind(this.onKeyEvent_, this));
  } else {
    // Get the initial captions state since we won't refresh the display
    // state in an API callback in this case.
    this.onCaptionsStateChanged_();
  }
};


/**
 * Dots representing a cursor.
 * @const
 * @private
 */
cvox.BrailleDisplayManager.CURSOR_DOTS_ = 1 << 6 | 1 << 7;


/**
 * @param {!cvox.NavBraille} content Content to send to the braille display.
 * @param {!cvox.ExpandingBrailleTranslator.ExpansionType} expansionType
 *     If the text has a {@code ValueSpan}, this indicates how that part
 *     of the display content is expanded when translating to braille.
 *     (See {@code cvox.ExpandingBrailleTranslator}).
 */
cvox.BrailleDisplayManager.prototype.setContent = function(
    content, expansionType) {
  this.translateContent_(content, expansionType);
};


/**
 * Sets the command listener.  When a command is invoked, the listener will be
 * called with the BrailleKeyEvent corresponding to the command and the content
 * that was present on the display when the command was invoked.  The content
 * is guaranteed to be identical to an object previously used as the parameter
 * to cvox.BrailleDisplayManager.setContent, or null if no content was set.
 * @param {function(!cvox.BrailleKeyEvent, !cvox.NavBraille)} func The listener.
 */
cvox.BrailleDisplayManager.prototype.setCommandListener = function(func) {
  this.commandListener_ = func;
};


/**
 * @param {!cvox.BrailleDisplayState} newState Display state reported
 *     by the extension API.
 * @private
 */
cvox.BrailleDisplayManager.prototype.refreshDisplayState_ = function(
    newState) {
  var oldColumnCount = this.displayState_.textColumnCount || 0;
  var oldRowCount = this.displayState_.textRowCount || 0;
  var processDisplayState = function(displayState) {
    this.displayState_ = displayState;
    var newColumnCount = displayState.textColumnCount || 0;
    var newRowCount = displayState.textRowCount || 0;

    if (oldColumnCount != newColumnCount || oldRowCount != newRowCount) {
      this.panStrategy_.setDisplaySize(newRowCount, newColumnCount);
    }
    this.refresh_();
  }.bind(this);
  this.realDisplayState_ = newState;
  if (newState.available) {
    processDisplayState(newState);
    // Update the dimensions of the virtual braille captions display to those
    // of a real physical display when one is plugged in.
  } else {
    cvox.BrailleCaptionsBackground.getVirtualDisplayState(processDisplayState);
  }
};


/**
 * Called when the state of braille captions changes.
 * @private
 */
cvox.BrailleDisplayManager.prototype.onCaptionsStateChanged_ = function() {
  // Force reevaluation of the display state based on our stored real
  // hardware display state, meaning that if a real display is connected,
  // that takes precedence over the state from the captions 'virtual' display.
  this.refreshDisplayState_(this.realDisplayState_);
};


/**
 * Refreshes what is shown on the physical braille display and the virtual
 * braille captions display.
 * @private
 */
cvox.BrailleDisplayManager.prototype.refresh_ = function() {
  if (!this.displayState_.available) {
    return;
  }
  var brailleBuf = this.panStrategy_.getCurrentBrailleViewportContents();
  var textBuf = this.panStrategy_.getCurrentTextViewportContents();
  if (this.realDisplayState_.available) {
    chrome.brailleDisplayPrivate.writeDots(brailleBuf,
        brailleBuf.byteLength, 1);
  }
  if (cvox.BrailleCaptionsBackground.isEnabled()) {
    cvox.BrailleCaptionsBackground.setContent(textBuf, brailleBuf,
        this.panStrategy_.brailleToText, this.panStrategy_.offsetsForSlices,
        this.displayState_.textRowCount, this.displayState_.textColumnCount);
  }
};

/**
 * @param {!cvox.NavBraille} newContent New display content.
 * @param {cvox.ExpandingBrailleTranslator.ExpansionType} newExpansionType
 *     How the value part of of the new content should be expanded
 *     with regards to contractions.
 * @private
 */
cvox.BrailleDisplayManager.prototype.translateContent_ = function(
    newContent, newExpansionType) {
  var writeTranslatedContent = function(cells, textToBraille, brailleToText) {
    this.content_ = newContent;
    this.expansionType_ = newExpansionType;
    var startIndex = this.content_.startIndex;
    var endIndex = this.content_.endIndex;
    var targetPosition;
    if (startIndex >= 0) {
      var translatedStartIndex;
      var translatedEndIndex;
      if (startIndex >= textToBraille.length) {
        // Allow the cells to be extended with one extra cell for
        // a carret after the last character.
        var extCells = new ArrayBuffer(cells.byteLength + 1);
        new Uint8Array(extCells).set(new Uint8Array(cells));
        // Last byte is initialized to 0.
        cells = extCells;
        translatedStartIndex = cells.byteLength - 1;
      } else {
        translatedStartIndex = textToBraille[startIndex];
      }
      if (endIndex >= textToBraille.length) {
        // endIndex can't be past-the-end of the last cell unless
        // startIndex is too, so we don't have to do another
        // extension here.
        translatedEndIndex = cells.byteLength;
      } else {
        translatedEndIndex = textToBraille[endIndex];
      }
      // Add the cursor to cells.
      this.writeCursor_(cells, translatedStartIndex, translatedEndIndex);
      targetPosition = translatedStartIndex;
    } else {
      targetPosition = 0;
    }
    this.panStrategy_.setContent(this.content_.text.toString(),
        cells, brailleToText, targetPosition);

    this.refresh_();
  }.bind(this);

  var translator = this.translatorManager_.getExpandingTranslator();
  if (!translator) {
    writeTranslatedContent(new ArrayBuffer(0), [], []);
  } else {
    translator.translate(
        newContent.text,
        newExpansionType,
        writeTranslatedContent);
  }
};


/**
 * @param {cvox.BrailleKeyEvent} event The key event.
 * @private
 */
cvox.BrailleDisplayManager.prototype.onKeyEvent_ = function(event) {
  switch (event.command) {
    case cvox.BrailleKeyCommand.PAN_LEFT:
      this.panLeft_();
      break;
    case cvox.BrailleKeyCommand.PAN_RIGHT:
      this.panRight_();
      break;
    case cvox.BrailleKeyCommand.ROUTING:
      event.displayPosition = this.brailleToTextPosition_(
          event.displayPosition + this.panStrategy_.viewPort.firstRow *
          this.panStrategy_.displaySize.columns);
      // fall through
    default:
      this.commandListener_(event, this.content_);
      break;
  }
};


/**
 * Shift the display by one full display size and refresh the content.
 * Sends the appropriate command if the display is already at the leftmost
 * position.
 * @private
 */
cvox.BrailleDisplayManager.prototype.panLeft_ = function() {
  if (this.panStrategy_.previous()) {
    this.refresh_();
  } else {
    this.commandListener_({
      command: cvox.BrailleKeyCommand.PAN_LEFT
    }, this.content_);
  }
};


/**
 * Shifts the display position to the right by one full display size and
 * refreshes the content.  Sends the appropriate command if the display is
 * already at its rightmost position.
 * @private
 */
cvox.BrailleDisplayManager.prototype.panRight_ = function() {
  if (this.panStrategy_.next()) {
    this.refresh_();
  } else {
    this.commandListener_({
      command: cvox.BrailleKeyCommand.PAN_RIGHT
    }, this.content_);
  }
};

/**
 * Writes a cursor in the specified range into translated content.
 * @param {ArrayBuffer} buffer Buffer to add cursor to.
 * @param {number} startIndex The start index to place the cursor.
 * @param {number} endIndex The end index to place the cursor (exclusive).
 * @private
 */
cvox.BrailleDisplayManager.prototype.writeCursor_ = function(
    buffer, startIndex, endIndex) {
  if (startIndex < 0 || startIndex >= buffer.byteLength ||
      endIndex < startIndex || endIndex > buffer.byteLength) {
    return;
  }
  if (startIndex == endIndex) {
    endIndex = startIndex + 1;
  }
  var dataView = new DataView(buffer);
  while (startIndex < endIndex) {
    var value = dataView.getUint8(startIndex);
    value |= cvox.BrailleDisplayManager.CURSOR_DOTS_;
    dataView.setUint8(startIndex, value);
    startIndex++;
  }
};

/**
 * Returns the text position corresponding to an absolute braille position,
 * that is not accounting for the current pan position.
 * @private
 * @param {number} braillePosition Braille position relative to the startof
 *        the translated content.
 * @return {number} The mapped position in code units.
 */
cvox.BrailleDisplayManager.prototype.brailleToTextPosition_ =
    function(braillePosition) {
  var mapping = this.panStrategy_.brailleToText;
  if (braillePosition < 0) {
    // This shouldn't happen.
    console.error('WARNING: Braille position < 0: ' + braillePosition);
    return 0;
  } else if (braillePosition >= mapping.length) {
    // This happens when the user clicks on the right part of the display
    // when it is not entirely filled with content.  Allow addressing the
    // position after the last character.
    return this.content_.text.length;
  } else {
    return mapping[braillePosition];
  }
};

/**
 * @param {boolean} wordWrap
 * @private
 */
cvox.BrailleDisplayManager.prototype.updatePanStrategy_ = function(wordWrap) {
  this.panStrategy_.setPanStrategy(wordWrap);
  this.refresh_();
};
