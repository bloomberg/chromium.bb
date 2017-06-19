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
    var useRichText = editing.useRichText && node.state.richlyEditable;

    /** @private {!AutomationEditableText} */
    this.editableText_ = useRichText ? new AutomationRichEditableText(node) :
                                       new AutomationEditableText(node);
  }.bind(this));
}

TextFieldTextEditHandler.prototype = {
  __proto__: editing.TextEditHandler.prototype,

  /** @override */
  onEvent: function(evt) {
    if (evt.type !== EventType.TEXT_CHANGED &&
        evt.type !== EventType.TEXT_SELECTION_CHANGED &&
        evt.type !== EventType.VALUE_CHANGED && evt.type !== EventType.FOCUS)
      return;
    if (!evt.target.state.focused || !evt.target.state.editable ||
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
      this, node.value || '', Math.min(start, end), Math.max(start, end),
      node.state[StateType.PROTECTED] /**password*/, cvox.ChromeVox.tts);
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
        newValue, this.node_.textSelStart || 0, this.node_.textSelEnd || 0,
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

  this.line_ = new editing.EditableLine(
      root.anchorObject, root.anchorOffset, root.focusObject, root.focusOffset);
}

AutomationRichEditableText.prototype = {
  __proto__: AutomationEditableText.prototype,

  /** @override */
  onUpdate: function() {
    var root = this.node_.root;
    if (!root.anchorObject || !root.focusObject)
      return;

    var cur = new editing.EditableLine(
        root.anchorObject, root.anchorOffset || 0, root.focusObject,
        root.focusOffset || 0);
    var prev = this.line_;
    this.line_ = cur;

    if (prev.equals(cur)) {
      // Collapsed cursor.
      this.changed(new cvox.TextChangeEvent(
          cur.text || '', cur.startOffset || 0, cur.endOffset || 0, true));
      this.brailleCurrentRichLine_();

      // Finally, queue up any text markers/styles at bounds.
      var container = cur.startContainer_;
      if (!container)
        return;

      if (container.markerTypes) {
        // Only consider markers that start or end at the selection bounds.
        var markerStartIndex = -1, markerEndIndex = -1;
        var localStartOffset = cur.localStartOffset;
        for (var i = 0; i < container.markerStarts.length; i++) {
          if (container.markerStarts[i] == localStartOffset) {
            markerStartIndex = i;
            break;
          }
        }

        var localEndOffset = cur.localEndOffset;
        for (var i = 0; i < container.markerEnds.length; i++) {
          if (container.markerEnds[i] == localEndOffset) {
            markerEndIndex = i;
            break;
          }
        }

        if (markerStartIndex > -1)
          this.speakTextMarker_(container.markerTypes[markerStartIndex]);

        if (markerEndIndex > -1)
          this.speakTextMarker_(container.markerTypes[markerEndIndex], true);
      }

      // Start of the container.
      if (cur.containerStartOffset == cur.startOffset)
        this.speakTextStyle_(container);
      else if (cur.containerEndOffset == cur.endOffset)
        this.speakTextStyle_(container, true);

      return;
    }

    // Just output the current line.
    cvox.ChromeVox.tts.speak(cur.text, cvox.QueueMode.CATEGORY_FLUSH);
    this.brailleCurrentRichLine_();

    // The state in EditableTextBase needs to get updated with the new line
    // contents, so that subsequent intra-line changes get the right state
    // transitions.
    this.value = cur.text;
    this.start = cur.startOffset;
    this.end = cur.endOffset;
  },

  /**
   * @param {number} markerType
   * @param {boolean=} opt_end
   * @private
   */
  speakTextMarker_: function(markerType, opt_end) {
    // TODO(dtseng): Plumb through constants to automation.
    var msgs = [];
    if (markerType & 1)
      msgs.push(opt_end ? 'misspelling_end' : 'misspelling_start');
    if (markerType & 2)
      msgs.push(opt_end ? 'grammar_end' : 'grammar_start');
    if (markerType & 4)
      msgs.push(opt_end ? 'text_match_end' : 'text_match_start');

    if (msgs.length) {
      msgs.forEach(function(msg) {
        cvox.ChromeVox.tts.speak(
            Msgs.getMsg(msg), cvox.QueueMode.QUEUE,
            cvox.AbstractTts.PERSONALITY_ANNOTATION);
      });
    }
  },

  /**
   * @param {!AutomationNode} style
   * @param {boolean=} opt_end
   * @private
   */
  speakTextStyle_: function(style, opt_end) {
    var msgs = [];
    if (style.bold)
      msgs.push(opt_end ? 'bold_end' : 'bold_start');
    if (style.italic)
      msgs.push(opt_end ? 'italic_end' : 'italic_start');
    if (style.underline)
      msgs.push(opt_end ? 'underline_end' : 'underline_start');
    if (style.lineThrough)
      msgs.push(opt_end ? 'line_through_end' : 'line_through_start');

    if (msgs.length) {
      msgs.forEach(function(msg) {
        cvox.ChromeVox.tts.speak(
            Msgs.getMsg(msg), cvox.QueueMode.QUEUE,
            cvox.AbstractTts.PERSONALITY_ANNOTATION);
      });
    }
  },

  /** @private */
  brailleCurrentRichLine_: function() {
    var cur = this.line_;
    var value = cur.value_;
    value.setSpan(new cvox.ValueSpan(0), 0, cur.value_.length);
    value.setSpan(
        new cvox.ValueSelectionSpan(), cur.startOffset, cur.endOffset);
    cvox.ChromeVox.braille.write(new cvox.NavBraille(
        {text: value, startIndex: cur.startOffset, endIndex: cur.endOffset}));
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
    return 0;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    return 0;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    return this.value.length;
  },

  /** @override */
  getLineBreaks_: function() {
    return [];
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

/**
 * An EditableLine encapsulates all data concerning a line in the automation
 * tree necessary to provide output.
 * @constructor
 */
editing.EditableLine = function(startNode, startIndex, endNode, endIndex) {
  /** @private {!Cursor} */
  this.start_ = new Cursor(startNode, startIndex);
  this.start_ = this.start_.deepEquivalent || this.start_;

  /** @private {!Cursor} */
  this.end_ = new Cursor(endNode, endIndex);
  this.end_ = this.end_.deepEquivalent || this.end_;
  /** @private {number} */
  this.localContainerStartOffset_ = startIndex;

  // Computed members.
  /** @private {Spannable} */
  this.value_;
  /** @private {AutomationNode} */
  this.lineStart_;
  /** @private {AutomationNode} */
  this.lineEnd_;
  /** @private {AutomationNode|undefined} */
  this.startContainer_;
  /** @private {AutomationNode|undefined} */
  this.lineStartContainer_;
  /** @private {number} */
  this.localLineStartContainerOffset_ = 0;

  this.computeLineData_();
};

editing.EditableLine.prototype = {
  /** @private */
  computeLineData_: function() {
    var nameLen = 0;
    if (this.start_.node.name)
      nameLen = this.start_.node.name.length;

    this.value_ = new Spannable(this.start_.node.name || '', this.start_);
    if (this.start_.node == this.end_.node)
      this.value_.setSpan(this.end_, 0, nameLen);

    // Initialize defaults.
    this.lineStart_ = this.start_.node;
    this.lineEnd_ = this.lineStart_;
    this.startContainer_ = this.lineStart_.parent;
    this.lineStartContainer_ = this.lineStart_.parent;

    // Annotate each chunk with its associated inline text box node.
    this.value_.setSpan(this.lineStart_, 0, this.lineStart_.name.length);

    // If the current selection is not on an inline text box (e.g. an image),
    // return early here so that the line contents are just the node. This is
    // pending the ability to show non-text leaf inline objects.
    if (this.lineStart_.role != RoleType.INLINE_TEXT_BOX)
      return;

    // Also, track their static text parents.
    var parents = [this.startContainer_];

    // Compute the start of line.
    var lineStart = this.lineStart_;
    while (lineStart.previousOnLine && lineStart.previousOnLine.role) {
      lineStart = lineStart.previousOnLine;
      if (lineStart.role != RoleType.INLINE_TEXT_BOX)
        continue;

      this.lineStart_ = lineStart;
      if (parents[0] != lineStart.parent)
        parents.unshift(lineStart.parent);

      var prepend = new Spannable(lineStart.name, lineStart);
      prepend.append(this.value_);
      this.value_ = prepend;
    }
    this.lineStartContainer_ = this.lineStart_.parent;

    var lineEnd = this.lineEnd_;
    while (lineEnd.nextOnLine && lineEnd.nextOnLine.role) {
      lineEnd = lineEnd.nextOnLine;
      if (lineEnd.role != RoleType.INLINE_TEXT_BOX)
        continue;

      this.lineEnd_ = lineEnd;
      if (parents[parents.length - 1] != lineEnd.parent)
        parents.push(this.lineEnd_.parent);

      var annotation = lineEnd;
      if (lineEnd == this.end_.node)
        annotation = this.end_;

      this.value_.append(new Spannable(lineEnd.name, annotation));
    }

    // Finally, annotate with all parent static texts as NodeSpan's so that
    // braille routing can key properly into the node with an offset.
    // Note that both line start and end needs to account for
    // potential offsets into the static texts as follows.
    var textCountBeforeLineStart = 0, textCountAfterLineEnd = 0;
    var finder = this.lineStart_;
    while (finder.previousSibling) {
      finder = finder.previousSibling;
      textCountBeforeLineStart += finder.name.length;
    }
    this.localLineStartContainerOffset_ = textCountBeforeLineStart;

    finder = this.lineEnd_;
    while (finder.nextSibling) {
      finder = finder.nextSibling;
      textCountAfterLineEnd += finder.name.length;
    }

    var len = 0;
    for (var i = 0; i < parents.length; i++) {
      var parent = parents[i];

      if (!parent.name)
        continue;

      var prevLen = len;

      var currentLen = parent.name.length;
      var offset = 0;

      // Subtract off the text count before when at the start of line.
      if (i == 0) {
        currentLen -= textCountBeforeLineStart;
        offset = textCountBeforeLineStart;
      }

      // Subtract text count after when at the end of the line.
      if (i == parents.length - 1)
        currentLen -= textCountAfterLineEnd;

      len += currentLen;

      try {
        this.value_.setSpan(new Output.NodeSpan(parent, offset), prevLen, len);

        // Also, annotate this span if it is associated with line containre.
        if (parent == this.startContainer_)
          this.value_.setSpan(parent, prevLen, len);
      } catch (e) {
      }
    }
  },

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get startOffset() {
    return this.value_.getSpanStart(this.start_) + this.start_.index;
  },

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get endOffset() {
    return this.value_.getSpanStart(this.end_) + this.end_.index;
  },

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localStartOffset() {
    return this.startOffset - this.containerStartOffset;
  },

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localEndOffset() {
    return this.endOffset - this.containerStartOffset;
  },

  /**
   * Gets the start offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   * @return {number}
   */
  get containerStartOffset() {
    return this.value_.getSpanStart(this.startContainer_);
  },

  /**
   * Gets the end offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   * @return {number}
   */
  get containerEndOffset() {
    return this.value_.getSpanEnd(this.startContainer_) - 1;
  },

  /**
   * The text content of this line.
   * @return {string} The text of this line.
   */
  get text() {
    return this.value_.toString();
  },

  /**
   * Returns true if |otherLine| surrounds the same line as |this|. Note that
   * the contents of the line might be different.
   * @return {boolean}
   */
  equals: function(otherLine) {
    // Equality is intentionally loose here as any of the state nodes can be
    // invalidated at any time. We rely upon the start/anchor of the line
    // staying the same.
    return otherLine.lineStartContainer_ == this.lineStartContainer_ &&
        otherLine.localLineStartContainerOffset_ ==
        this.localLineStartContainerOffset_;
  }
};

});
