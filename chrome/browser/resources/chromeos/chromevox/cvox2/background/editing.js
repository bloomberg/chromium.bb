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
goog.require('cvox.BrailleBackground');
goog.require('cvox.ChromeVoxEditableTextBase');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Cursor = cursors.Cursor;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var Range = cursors.Range;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;
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

/**
 * Flag set to indicate whether ChromeVox uses experimental rich text support.
 * @type {boolean}
 */
editing.useRichText = false;

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
   * @param {!(AutomationEvent|CustomAutomationEvent)} evt
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

  chrome.automation.getDesktop(function(desktop) {
    var useRichText = editing.useRichText &&
        node.state.richlyEditable;

    /** @private {!AutomationEditableText} */
    this.editableText_ = useRichText ?
        new AutomationRichEditableText(node) : new AutomationEditableText(node);
  }.bind(this));
}

TextFieldTextEditHandler.prototype = {
  __proto__: editing.TextEditHandler.prototype,

  /** @override */
  onEvent: function(evt) {
    if (evt.type !== EventType.TEXT_CHANGED &&
        evt.type !== EventType.TEXT_SELECTION_CHANGED &&
        evt.type !== EventType.VALUE_CHANGED &&
        evt.type !== EventType.FOCUS)
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
      node.value || '',
      Math.min(start, end),
      Math.max(start, end),
      node.state[StateType.PROTECTED] /**password*/,
      cvox.ChromeVox.tts);
  /** @override */
  this.multiline = node.state[StateType.MULTILINE] || false;
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
    var newValue = this.node_.value || '';

    if (this.value != newValue)
      this.lineBreaks_ = [];

    var textChangeEvent = new cvox.TextChangeEvent(
        newValue,
        this.node_.textSelStart || 0,
        this.node_.textSelEnd || 0,
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_();
  },

  /** @override */
  getLineIndex: function(charIndex) {
    if (!this.multiline)
      return 0;
    var breaks = this.node_.lineBreaks || [];
    var index = 0;
    while (index < breaks.length && breaks[index] <= charIndex)
      ++index;
    return index;
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
    // node.lineBreaks is undefined when the multiline field has no line
    // breaks.
    return this.node_.lineBreaks || [];
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
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields using anchor and focus selection.
 * @constructor
 * @param {!AutomationNode} node
 * @extends {AutomationEditableText}
 */
function AutomationRichEditableText(node) {
  AutomationEditableText.call(this, node);

  var root = this.node_.root;
  if (!root || !root.anchorObject || !root.focusObject)
    return;

  this.range = new cursors.Range(
      new cursors.Cursor(root.anchorObject, root.anchorOffset || 0),
      new cursors.Cursor(root.focusObject, root.focusOffset || 0));
}

AutomationRichEditableText.prototype = {
  __proto__: AutomationEditableText.prototype,

  /** @override */
  onUpdate: function() {
    var root = this.node_.root;
    if (!root.anchorObject || !root.focusObject)
      return;

    var cur = new cursors.Range(
        new cursors.Cursor(root.anchorObject, root.anchorOffset || 0),
        new cursors.Cursor(root.focusObject, root.focusOffset || 0));
    var prev = this.range;

    this.range = cur;

    if (prev.start.node == cur.start.node &&
        prev.end.node == cur.end.node &&
        cur.start.node == cur.end.node) {
      // Plain text: diff the two positions.
      this.changed(new cvox.TextChangeEvent(
          root.anchorObject.name || '',
          root.anchorOffset || 0,
          root.focusOffset || 0,
          true));

      var lineIndex = this.getLineIndex(this.start);
      var brailleLineStart = this.getLineStart(lineIndex);
      var brailleLineEnd = this.getLineEnd(lineIndex);
      var buff = new Spannable(this.value);
      buff.setSpan(new cvox.ValueSpan(0), brailleLineStart, brailleLineEnd);

      var selStart = this.start - brailleLineStart;
      var selEnd = this.end - brailleLineStart;
      buff.setSpan(new cvox.ValueSelectionSpan(), selStart, selEnd);
      cvox.ChromeVox.braille.write(new cvox.NavBraille({text: buff,
          startIndex: selStart,
          endIndex: selEnd}));
      return;
    } else {
      // Rich text:
      // If the position is collapsed, expand to the current line.
      var start = cur.start;
      var end = cur.end;
      if (start.equals(end)) {
        start = start.move(Unit.LINE, Movement.BOUND, Dir.BACKWARD);
        end = end.move(Unit.LINE, Movement.BOUND, Dir.FORWARD);
      }
      var range = new cursors.Range(start, end);
      var output = new Output().withRichSpeechAndBraille(
          range, prev, Output.EventType.NAVIGATE);

      // This position is not describable.
      if (!output.hasSpeech) {
        cvox.ChromeVox.tts.speak('blank', cvox.QueueMode.CATEGORY_FLUSH);
        cvox.ChromeVox.braille.write(
            new cvox.NavBraille({text: '', startIndex: 0, endIndex: 0}));
      } else {
        output.go();
      }
    }

    // Keep the other members in sync for any future editable text base state
    // machine changes.
    this.value = cur.start.node.name || '';
    this.start = cur.start.index;
    this.end = cur.start.index;
  },

  /** @override */
  describeSelectionChanged: function(evt) {
    // Ignore end of text announcements.
    if ((this.start + 1) == evt.start && evt.start == this.value.length)
      return;

    cvox.ChromeVoxEditableTextBase.prototype.describeSelectionChanged.call(
        this, evt);
  },

  /** @override */
  getLineIndex: function(charIndex) {
    var breaks = this.getLineBreaks_();
    var index = 0;
    while (index < breaks.length && breaks[index] <= charIndex)
      ++index;
    return index;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    if (lineIndex == 0)
      return 0;
    var breaks = this.getLineBreaks_();
    return breaks[lineIndex - 1] ||
        this.node_.root.focusObject.value.length;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    var breaks = this.getLineBreaks_();
    var value = this.node_.root.focusObject.name;
    if (lineIndex >= breaks.length)
      return value.length;
    return breaks[lineIndex];
  },

  /** @override */
  getLineBreaks_: function() {
    return this.node_.root.focusObject.lineStartOffsets || [];
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
    if (testNode.state[StateType.FOCUSED] && testNode.state[StateType.EDITABLE])
      rootFocusedEditable = testNode;
    testNode = testNode.parent;
  } while (testNode);

  if (rootFocusedEditable)
    return new TextFieldTextEditHandler(rootFocusedEditable);

  return null;
};

/**
 * An observer that reacts to ChromeVox range changes that modifies braille
 * table output when over email or url text fields.
 * @constructor
 * @implements {ChromeVoxStateObserver}
 */
editing.EditingChromeVoxStateObserver = function() {
  ChromeVoxState.addObserver(this);
};

editing.EditingChromeVoxStateObserver.prototype = {
  __proto__: ChromeVoxStateObserver,

  /** @override */
  onCurrentRangeChanged: function(range) {
    var inputType = range && range.start.node.inputType;
    if (inputType == 'email' || inputType == 'url') {
      cvox.BrailleBackground.getInstance().getTranslatorManager().refresh(
          localStorage['brailleTable8']);
      return;
    }
    cvox.BrailleBackground.getInstance().getTranslatorManager().refresh(
        localStorage['brailleTable']);
  }
};

/**
 * @private {ChromeVoxStateObserver}
 */
editing.observer_ = new editing.EditingChromeVoxStateObserver();

});
