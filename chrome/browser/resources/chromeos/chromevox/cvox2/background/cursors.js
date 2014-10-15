// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes related to cursors that point to and select parts of
 * the automation tree.
 */

goog.provide('cursors.Cursor');
goog.provide('cursors.Movement');
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
  /** @type {!AutomationNode} */
  this.node = node;
  /** @type {number} */
  this.index = index;
};

cursors.Cursor.prototype = {
  /**
   * Returns a copy of this cursor.
   * @return {cursors.Cursor}
   */
  clone: function() {
    return new cursors.Cursor(this.node, this.index);
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
    var node = opt_node || this.node;
    return node.attributes.name || node.attributes.value || '';
  },

  /**
   * Moves this cursor by the unit in the given direction using the given
   * movement type.
   * @param {Unit} unit
   * @param {Movement} movement
   * @param {Dir} dir
   */
  move: function(unit, movement, dir) {
    switch (unit) {
      case Unit.CHARACTER:
        // BOUND and DIRECTIONAL are the same for characters.
        var node = this.node;
        var nextIndex = dir == Dir.FORWARD ? this.index + 1 : this.index - 1;
        if (nextIndex < 0 || nextIndex >= this.getText().length) {
          node = AutomationUtil.findNextNode(
              node, dir, AutomationPredicate.leaf);
          if (node) {
            nextIndex =
                dir == Dir.FORWARD ? 0 : this.getText(node).length - 1;
          } else {
            node = this.node;
            nextIndex = this.index;
          }
        }
        this.node = node;
        this.index = nextIndex;
        break;
      case Unit.WORD:
        switch (movement) {
          case Movement.BOUND:
            if (this.node.role == Role.inlineTextBox) {
              var start, end;
              for (var i = 0; i < this.node.attributes.wordStarts.length; i++) {
                if (this.index >= this.node.attributes.wordStarts[i] &&
                    this.index <= this.node.attributes.wordEnds[i]) {
                  start = this.node.attributes.wordStarts[i];
                  end = this.node.attributes.wordEnds[i];
                  break;
                }
              }
              if (goog.isDef(start) && goog.isDef(end))
                this.index = dir == Dir.FORWARD ? end : start;
            } else {
              // TODO(dtseng): Figure out what to do in this case.
            }
            break;
          case Movement.DIRECTIONAL:
            if (this.node.role == Role.inlineTextBox) {
              var start, end;
              for (var i = 0; i < this.node.attributes.wordStarts.length; i++) {
                if (this.index >= this.node.attributes.wordStarts[i] &&
                    this.index <= this.node.attributes.wordEnds[i]) {
                  var nextIndex = dir == Dir.FORWARD ? i + 1 : i - 1;
                  start = this.node.attributes.wordStarts[nextIndex];
                  end = this.node.attributes.wordEnds[nextIndex];
                  break;
                }
              }
              if (goog.isDef(start)) {
                this.index = start;
              } else {
                var node = AutomationUtil.findNextNode(this.node, dir,
                            AutomationPredicate.leaf);
                if (node) {
                  this.node = node;
                  this.index = 0;
                  if (dir == Dir.BACKWARD && node.role == Role.inlineTextBox) {
                    var starts = node.attributes.wordStarts;
                    this.index = starts[starts.length - 1] || 0;
                  } else {
                    // TODO(dtseng): Figure out what to do for general nodes.
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
            this.index = dir == Dir.FORWARD ? this.getText().length - 1 : 0;
            break;
          case Movement.DIRECTIONAL:
            this.node = AutomationUtil.findNextNode(
                this.node, dir, AutomationPredicate.leaf) || this.node;
            this.index = cursors.NODE_INDEX;
            break;
        }
        break;
      case Unit.LINE:
        this.index = 0;
        switch (movement) {
          case Movement.BOUND:
            var node = this.node;
            node = AutomationUtil.findNodeUntil(node, dir,
                AutomationPredicate.linebreak, {before: true});
            this.node = node || this.node;
            break;
          case Movement.DIRECTIONAL:
            var node = this.node;
            node = AutomationUtil.findNodeUntil(
                node, dir, AutomationPredicate.linebreak);

            // We stick to the beginning of lines out of convention.
            if (node && dir == Dir.BACKWARD) {
              node = AutomationUtil.findNodeUntil(node, dir,
                  AutomationPredicate.linebreak, {before: true}) || node;
            }
            this.node = node || this.node;
            break;
          }
        break;
    }
  }
};

});  // goog.scope
