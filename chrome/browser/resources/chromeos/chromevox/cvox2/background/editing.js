// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Processes events related to editing text and emits the
 * appropriate spoken and braille feedback.
 */

goog.provide('editing.TextEditHandler');

goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('Output');
goog.require('Output.EventType');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cvox.ChromeVoxEditableTextBase');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Cursor = cursors.Cursor;
var Dir = AutomationUtil.Dir;
var EventType = chrome.automation.EventType;
var Range = cursors.Range;
var RoleType = chrome.automation.RoleType;
var Movement = cursors.Movement;
var Unit = cursors.Unit;

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 * @constructor
 * @param {!AutomationNode} node
 */
editing.TextEditHandler = function(node) {
  /** @const {!AutomationNode} @private */
  this.node_ = node;
};

editing.TextEditHandler.prototype = {
  /** @return {!AutomationNode} */
  get node() {
    return this.node_;
  },

  /**
   * Receives the following kinds of events when the node provided to the
   * constructor is focuse: |focus|, |textChanged|, |textSelectionChanged| and
   * |valueChanged|.
   * An implementation of this method should emit the appropritate braille and
   * spoken feedback for the event.
   * @param {!AutomationEvent} evt
   */
  onEvent: goog.abstractMethod,
};

/**
 * A |TextEditHandler| suitable for text fields.
 * @constructor
 * @param {!AutomationNode} node A node with the role of |textField|
 * @extends {editing.TextEditHandler}
 */
function TextFieldTextEditHandler(node) {
  editing.TextEditHandler.call(this, node);
  /** @type {AutomationEditableText} @private */
  this.editableText_ = new AutomationEditableText(node);
}

TextFieldTextEditHandler.prototype = {
  __proto__: editing.TextEditHandler.prototype,

  /** @override */
  onEvent: function(evt) {
    if (evt.type !== EventType.textChanged &&
        evt.type !== EventType.textSelectionChanged &&
        evt.type !== EventType.valueChanged &&
        evt.type !== EventType.focus)
      return;
    if (!evt.target.state.focused ||
        !evt.target.state.editable ||
        evt.target != this.node_)
      return;

    this.editableText_.onUpdate();
  },
};

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 * @constructor
 * @param {!AutomationNode} node
 * @extends {cvox.ChromeVoxEditableTextBase}
 */
function AutomationEditableText(node) {
  if (!node.state.editable)
    throw Error('Node must have editable state set to true.');
  var start = node.textSelStart;
  var end = node.textSelEnd;
  cvox.ChromeVoxEditableTextBase.call(
      this,
      node.value,
      Math.min(start, end),
      Math.max(start, end),
      node.state.protected /**password*/,
      cvox.ChromeVox.tts);
  /** @override */
  this.multiline = node.state.multiline || false;
  /** @type {!AutomationNode} @private */
  this.node_ = node;
  /** @type {Array<number>} @private */
  this.lineBreaks_ = [];
}

AutomationEditableText.prototype = {
  __proto__: cvox.ChromeVoxEditableTextBase.prototype,

  /**
   * Called when the text field has been updated.
   */
  onUpdate: function() {
    var newValue = this.node_.value;

    if (this.value != newValue)
      this.lineBreaks_ = [];

    var textChangeEvent = new cvox.TextChangeEvent(
        newValue,
        this.node_.textSelStart,
        this.node_.textSelEnd,
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_();
  },

  /** @override */
  getLineIndex: function(charIndex) {
    if (!this.multiline)
      return 0;
    var breaks = this.getLineBreaks_();

    var computedIndex = 0;
    for (var index = 0; index < breaks.length; index++) {
      if (charIndex >= breaks[index] &&
          (index == (breaks.length - 1) ||
              charIndex < breaks[index + 1])) {
        computedIndex = index + 1;
        break;
      }
    }
    return computedIndex;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    if (!this.multiline || lineIndex == 0)
      return 0;
    var breaks = this.getLineBreaks_();
    return breaks[lineIndex - 1] || this.node_.value.length;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    var breaks = this.getLineBreaks_();
    var value = this.node_.value;
    if (lineIndex >= breaks.length)
      return value.length;
    return breaks[lineIndex] - 1;
  },

  /**
   * @return {Array<number>}
   * @private
   */
  getLineBreaks_: function() {
    if (this.lineBreaks_.length)
      return this.lineBreaks_;

    // |lineStartOffsets| is sometimes pretty wrong especially when
    // there's multiple consecutive line breaks.

    // Use Blink's innerText which we plumb through as value.  For
    // soft line breaks, use line start offsets, but ensure it is
    // likely to be a soft line wrap.

    var lineStartOffsets = {};
    this.node_.lineStartOffsets.forEach(function(offset) {
      lineStartOffsets[offset] = true;
    });
    var lineBreaks = [];
    for (var i = 0; i < this.node_.value.length; i++) {
      var prev = this.node_.value[i - 1];
      var next = this.node_.value[i + 1];
      var cur = this.node_.value[i];
      if (prev == '\n') {
        // Hard line break.
        lineBreaks.push(i);
      } else if (lineStartOffsets[i]) {
        // Soft line break.
        if (prev != '\n' && next != '\n' && cur != '\n')
          lineBreaks.push(i);
      }
    }

    this.lineBreaks_ = lineBreaks;

    return lineBreaks;
  },

  /** @private */
  outputBraille_: function() {
    var isFirstLine = false;  // First line in a multiline field.
    var output = new Output();
    var range;
    if (this.multiline) {
      var lineIndex = this.getLineIndex(this.start);
      if (lineIndex == 0) {
        isFirstLine = true;
        output.formatForBraille('$name', this.node_);
      }
      range = new Range(
          new Cursor(this.node_, this.getLineStart(lineIndex)),
          new Cursor(this.node_, this.getLineEnd(lineIndex)));
    } else {
      range = Range.fromNode(this.node_);
    }
    output.withBraille(range, null, Output.EventType.NAVIGATE);
    if (isFirstLine)
      output.formatForBraille('@tag_textarea');
    output.go();
  }
};

/**
 * @param {!AutomationNode} node The root editable node, i.e. the root of a
 *     contenteditable subtree or a text field.
 * @return {editing.TextEditHandler}
 */
editing.TextEditHandler.createForNode = function(node) {
  var rootFocusedEditable = null;
  var testNode = node;

  do {
    if (testNode.state.focused && testNode.state.editable)
      rootFocusedEditable = testNode;
    testNode = testNode.parent;
  } while (testNode);

  if (rootFocusedEditable)
    return new TextFieldTextEditHandler(rootFocusedEditable);

  return null;
};

});
