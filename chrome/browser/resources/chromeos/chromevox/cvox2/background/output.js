// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */

goog.provide('Output');

goog.require('AutomationUtil.Dir');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');

/** @constructor */
Output = function() {
  /** @type {cursors.Range} @private */
  this.prevRange_ = null;
};

Output.prototype = {
  /**
   * Handles output of the given range.
   */
  output: function(range) {
    var out = {};
    if (range.isSubNode())
    out = this.subNode_(range);
  else
    out = this.node_(range);

    cvox.ChromeVox.tts.speak(out.text, cvox.QueueMode.FLUSH);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(out.text));
    chrome.accessibilityPrivate.setFocusRing(out.locations);

    this.prevRange_ = range;
  },

  /**
   * @param {!cursors.Range} range
   * @return {{text: string, locations: !Array.<Object>}}
   * @private
   */
  node_: function(range) {
    // Walk the range and collect descriptions.
    var out = {text: '', locations: []};
    var cursor = range.getStart();
    while (cursor.getNode() != range.getEnd().getNode()) {
      out.text += this.getCursorDesc_(cursor);
      out.locations.push(cursor.getNode().location);
      cursor = cursor.move(cursors.Unit.NODE,
                           cursors.Movement.DIRECTIONAL,
                           AutomationUtil.Dir.FORWARD);
    }
    out.text += this.getCursorDesc_(range.getEnd());
    out.locations.push(range.getEnd().getNode().location);
    return out;
  },

    /**
   * @param {!cursors.Range} range
   * @return {{text: string, locations: !Array.<Object>}}
   * @private
   */
  subNode_: function(range) {
    // TODO(dtseng): Need a way to select subnode ranges (or compute their
    // rects).
    var out = {text: '', locations: []};
    var startIndex = range.getStart().getIndex();
    var endIndex = range.getEnd().getIndex();
    if (startIndex === endIndex)
      endIndex++;

    out.text = range.getStart().getText().substring(startIndex, endIndex);

    return out;
  },

  // TODO(dtseng): This is just placeholder logic for generating descriptions
  // pending further design discussion.
  /**
   * @param {!cursors.Cursor} cursor
   * @return {string}
   * @private
   */
  getCursorDesc_: function(cursor) {
    var node = cursor.getNode();
    var container = node;
    while (container &&
        (container.role == chrome.automation.RoleType.inlineTextBox ||
        container.role == chrome.automation.RoleType.staticText))
      container = container.parent();

    var role = container ? container.role : node.role;
    return [node.attributes.name, node.attributes.value, role].join(', ');
  }
};
