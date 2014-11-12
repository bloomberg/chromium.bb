// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes related to cursors that point to and select parts of
 * the automation tree.
 */

goog.provide('cursors.Cursor');
goog.provide('cursors.Movement');
goog.provide('cursors.Range');
goog.provide('cursors.Unit');

goog.require('AutomationUtil');

/**
 * The special index that represents a cursor pointing to a node without
 * pointing to any part of its accessible text.
 */
cursors.NODE_INDEX = -1;

/**
 * Represents units of CursorMovement.
 * @enum {string}
 */
cursors.Unit = {
  /** A single character within accessible name or value. */
  CHARACTER: 'character',

  /** A range of characters (given by attributes on automation nodes). */
  WORD: 'word',

  /** A leaf node. */
  NODE: 'node',

  /** Formed by a set of leaf nodes that are inline. */
  LINE: 'line'
};

/**
 * Represents the ways in which cursors can move given a cursor unit.
 * @enum {string}
 */
cursors.Movement = {
  /** Move to the beginning or end of the current unit. */
  BOUND: 'bound',

  /** Move to the next unit in a particular direction. */
  DIRECTIONAL: 'directional'
};

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = AutomationUtil.Dir;
var Movement = cursors.Movement;
var Role = chrome.automation.RoleType;
var Unit = cursors.Unit;

/**
 * Represents a position within the automation tree.
 * @constructor
 * @param {!AutomationNode} node
 * @param {number} index A 0-based index into either this cursor's name or value
 * attribute. Relies on the fact that a node has either a name or a value but
 * not both. An index of |cursors.NODE_INDEX| means the node as a whole is
 * pointed to and covers the case where the accessible text is empty.
 */
cursors.Cursor = function(node, index) {
  /** @type {!AutomationNode} @private */
  this.node_ = node;
  /** @type {number} @private */
  this.index_ = index;
};

/**
 * Convenience method to construct a Cursor from a node.
 * @param {!AutomationNode} node
 * @return {!cursors.Cursor}
 */
cursors.Cursor.fromNode = function(node) {
  return new cursors.Cursor(node, cursors.NODE_INDEX);
};

cursors.Cursor.prototype = {
  /**
   * Returns true if |rhs| is equal to this cursor.
   * @param {!cursors.Cursor} rhs
   * @return {boolean}
   */
  equals: function(rhs) {
    return this.node_ === rhs.getNode() &&
        this.index_ === rhs.getIndex();
  },

  /**
   * @return {!AutomationNode}
   */
  getNode: function() {
    return this.node_;
  },

  /**
   * @return {number}
   */
  getIndex: function() {
    return this.index_;
  },

  /**
   * Gets the accessible text of the node associated with this cursor.
   *
   * Note that only one of |name| or |value| attribute is ever nonempty on an
   * automation node. If either contains whitespace, we still treat it as we do
   * for a nonempty string.
   * @param {!AutomationNode=} opt_node Use this node rather than this cursor's
   * node.
   * @return {string}
   */
  getText: function(opt_node) {
    var node = opt_node || this.node_;
    return node.attributes.name || node.attributes.value || '';
  },

  /**
   * Makes a Cursor which has been moved from this cursor by the unit in the
   * given direction using the given movement type.
   * @param {Unit} unit
   * @param {Movement} movement
   * @param {Dir} dir
   * @return {!cursors.Cursor} The moved cursor.
   */
  move: function(unit, movement, dir) {
    var newNode = this.node_;
    var newIndex = this.index_;

    if (unit != Unit.NODE && newIndex === cursors.NODE_INDEX)
      newIndex = 0;

    switch (unit) {
      case Unit.CHARACTER:
        // BOUND and DIRECTIONAL are the same for characters.
        newIndex = dir == Dir.FORWARD ? newIndex + 1 : newIndex - 1;
        if (newIndex < 0 || newIndex >= this.getText().length) {
          newNode = AutomationUtil.findNextNode(
              newNode, dir, AutomationPredicate.leafWithText);
          if (newNode) {
            newIndex =
                dir == Dir.FORWARD ? 0 : this.getText(newNode).length - 1;
            newIndex = newIndex == -1 ? 0 : newIndex;
          } else {
            newIndex = this.index_;
          }
        }
        break;
      case Unit.WORD:
        switch (movement) {
          case Movement.BOUND:
            if (newNode.role == Role.inlineTextBox) {
              var start, end;
              for (var i = 0; i < newNode.attributes.wordStarts.length; i++) {
                if (newIndex >= newNode.attributes.wordStarts[i] &&
                    newIndex <= newNode.attributes.wordEnds[i]) {
                  start = newNode.attributes.wordStarts[i];
                  end = newNode.attributes.wordEnds[i];
                  break;
                }
              }
              if (goog.isDef(start) && goog.isDef(end))
                newIndex = dir == Dir.FORWARD ? end : start;
            } else {
              // TODO(dtseng): Figure out what to do in this case.
            }
            break;
          case Movement.DIRECTIONAL:
            if (newNode.role == Role.inlineTextBox) {
              var start, end;
              for (var i = 0; i < newNode.attributes.wordStarts.length; i++) {
                if (newIndex >= newNode.attributes.wordStarts[i] &&
                    newIndex <= newNode.attributes.wordEnds[i]) {
                  var nextIndex = dir == Dir.FORWARD ? i + 1 : i - 1;
                  start = newNode.attributes.wordStarts[nextIndex];
                  end = newNode.attributes.wordEnds[nextIndex];
                  break;
                }
              }
              if (goog.isDef(start)) {
                newIndex = start;
              } else {
                // The backward case is special at the beginning of nodes.
                if (dir == Dir.BACKWARD && newIndex != 0) {
                  newIndex = 0;
                } else {
                  newNode = AutomationUtil.findNextNode(newNode, dir,
                      AutomationPredicate.leaf);
                  if (newNode) {
                    newIndex = 0;
                    if (dir == Dir.BACKWARD &&
                        newNode.role == Role.inlineTextBox) {
                      var starts = newNode.attributes.wordStarts;
                      newIndex = starts[starts.length - 1] || 0;
                    } else {
                      // TODO(dtseng): Figure out what to do for general nodes.
                    }
                  }
                }
              }
            } else {
              // TODO(dtseng): Figure out what to do in this case.
            }
        }
        break;
      case Unit.NODE:
        switch (movement) {
          case Movement.BOUND:
            newIndex = dir == Dir.FORWARD ? this.getText().length - 1 : 0;
            break;
          case Movement.DIRECTIONAL:
            newNode = AutomationUtil.findNextNode(
                newNode, dir, AutomationPredicate.leaf) || this.node_;
            newIndex = cursors.NODE_INDEX;
            break;
        }
        break;
      case Unit.LINE:
        newIndex = 0;
        switch (movement) {
          case Movement.BOUND:
            newNode = AutomationUtil.findNodeUntil(newNode, dir,
                AutomationPredicate.linebreak, {before: true});
            newNode = newNode || this.node_;
            newIndex =
                dir == Dir.FORWARD ? this.getText(newNode).length : 0;
            break;
          case Movement.DIRECTIONAL:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak);
            break;
          }
      break;
      default:
        throw 'Unrecognized unit: ' + unit;
    }
    newNode = newNode || this.node_;
    newIndex = goog.isDef(newIndex) ? newIndex : this.index_;
    return new cursors.Cursor(newNode, newIndex);
  }
};

/**
 * Represents a range in the automation tree. There is no visible selection on
 * the page caused by usage of this object.
 * It is assumed that the caller provides |start| and |end| in document order.
 * @param {!cursors.Cursor} start
 * @param {!cursors.Cursor} end
 * @constructor
 */
cursors.Range = function(start, end) {
  /** @type {!cursors.Cursor} @private */
  this.start_ = start;
  /** @type {!cursors.Cursor} @private */
  this.end_ = end;
};

/**
 * Convenience method to construct a Range surrounding one node.
 * @param {!AutomationNode} node
 * @return {!cursors.Range}
 */
cursors.Range.fromNode = function(node) {
  var cursor = cursors.Cursor.fromNode(node);
  return new cursors.Range(cursor, cursor);
};

 /**
 * Given |rangeA| and |rangeB| in order, determine which |Dir|
 * relates them.
 * @param {!cursors.Range} rangeA
 * @param {!cursors.Range} rangeB
 * @return {Dir}
 */
cursors.Range.getDirection = function(rangeA, rangeB) {
  if (!rangeA || !rangeB)
    return Dir.FORWARD;

  // They are the same range.
  if (rangeA.getStart().getNode() === rangeB.getStart().getNode() &&
      rangeB.getEnd().getNode() === rangeA.getEnd().getNode())
    return Dir.FORWARD;

  var testDirA =
      AutomationUtil.getDirection(
          rangeA.getStart().getNode(), rangeB.getEnd().getNode());
  var testDirB =
      AutomationUtil.getDirection(
          rangeB.getStart().getNode(), rangeA.getEnd().getNode());

  // The two ranges are either partly overlapping or non overlapping.
  if (testDirA == Dir.FORWARD && testDirB == Dir.BACKWARD)
    return Dir.FORWARD;
  else if (testDirA == Dir.BACKWARD && testDirB == Dir.FORWARD)
    return Dir.BACKWARD;
  else
    return testDirA;
};

cursors.Range.prototype = {
  /**
   * Returns true if |rhs| is equal to this range.
   * @param {!cursors.Range} rhs
   * @return {boolean}
   */
  equals: function(rhs) {
    return this.start_.equals(rhs.getStart()) &&
        this.end_.equals(rhs.getEnd());
  },

  /**
   * Gets a cursor bounding this range.
   * @param {Dir} dir Which endpoint cursor to return; Dir.FORWARD for end,
   * Dir.BACKWARD for start.
   * @param {boolean=} opt_reverse Specify to have Dir.BACKWARD return end,
   * Dir.FORWARD return start.
   * @return {!cursors.Cursor}
   */
  getBound: function(dir, opt_reverse) {
    if (opt_reverse)
      return dir == Dir.BACKWARD ? this.end_ : this.start_;
    return dir == Dir.FORWARD ? this.end_ : this.start_;
  },

  /**
   * @return {!cursors.Cursor}
   */
  getStart: function() {
    return this.start_;
  },

  /**
   * @return {!cursors.Cursor}
   */
  getEnd: function() {
    return this.end_;
  },

  /**
   * Returns true if this range covers less than a node.
   * @return {boolean}
   */
  isSubNode: function() {
    return this.getStart().getNode() === this.getEnd().getNode() &&
        this.getStart().getIndex() > -1 &&
        this.getEnd().getIndex() > -1;
  },

  /**
   * Makes a Range which has been moved from this range by the given unit and
   * direction.
   * @param {Unit} unit
   * @param {Dir} dir
   * @return {cursors.Range}
   */
  move: function(unit, dir) {
    var newStart = this.start_;
    var newEnd = newStart;
    switch (unit) {
      case Unit.CHARACTER:
        newStart = newStart.move(unit, Movement.BOUND, dir);
        newEnd = newStart.move(unit, Movement.BOUND, Dir.FORWARD);
        // Character crossed a node; collapses to the end of the node.
        if (newStart.getNode() !== newEnd.getNode())
          newEnd = newStart;
        break;
      case Unit.WORD:
      case Unit.LINE:
        newStart = newStart.move(unit, Movement.DIRECTIONAL, dir);
        newStart = newStart.move(unit, Movement.BOUND, Dir.BACKWARD);
        newEnd = newStart.move(unit, Movement.BOUND, Dir.FORWARD);
        break;
      case Unit.NODE:
        newStart = newStart.move(unit, Movement.DIRECTIONAL, dir);
        newEnd = newStart;
        break;
    }
    return new cursors.Range(newStart, newEnd);
  }
};

});  // goog.scope
