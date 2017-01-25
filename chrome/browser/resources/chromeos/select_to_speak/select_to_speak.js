// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * Return the rect that encloses two points.
 * @param {number} x1 The first x coordinate.
 * @param {number} y1 The first y coordinate.
 * @param {number} x2 The second x coordinate.
 * @param {number} y2 The second x coordinate.
 * @return {{left: number, top: number, width: number, height: number}}
 */
function rectFromPoints(x1, y1, x2, y2) {
  var left = Math.min(x1, x2);
  var right = Math.max(x1, x2);
  var top = Math.min(y1, y2);
  var bottom = Math.max(y1, y2);
  return {left: left,
          top: top,
          width: right - left,
          height: bottom - top};
}

/**
 * Returns true if |rect1| and |rect2| overlap. The rects must define
 * left, top, width, and height.
 * @param {{left: number, top: number, width: number, height: number}} rect1
 * @param {{left: number, top: number, width: number, height: number}} rect2
 * @return {boolean} True if the rects overlap.
 */
function overlaps(rect1, rect2) {
  var l1 = rect1.left;
  var r1 = rect1.left + rect1.width;
  var t1 = rect1.top;
  var b1 = rect1.top + rect1.height;
  var l2 = rect2.left;
  var r2 = rect2.left + rect2.width;
  var t2 = rect2.top;
  var b2 = rect2.top + rect2.height;
  return (l1 < r2 && r1 > l2 && t1 < b2 && b1 > t2);
}

/**
 * @constructor
 */
var SelectToSpeak = function() {
  /** @private { AutomationNode } */
  this.node_ = null;

  /** @private { boolean } */
  this.down_ = false;

  /** @private {{x: number, y: number}} */
  this.mouseStart_ = {x: 0, y: 0};

  chrome.automation.getDesktop(function(desktop) {
    desktop.addEventListener(
        EventType.MOUSE_PRESSED, this.onMousePressed_.bind(this), true);
    desktop.addEventListener(
        EventType.MOUSE_DRAGGED, this.onMouseDragged_.bind(this), true);
    desktop.addEventListener(
        EventType.MOUSE_RELEASED, this.onMouseReleased_.bind(this), true);
    desktop.addEventListener(
        EventType.MOUSE_CANCELED, this.onMouseCanceled_.bind(this), true);
  }.bind(this));
};

SelectToSpeak.prototype = {
  /**
   * Called when the mouse is pressed and the user is in a mode where
   * select-to-speak is capturing mouse events (for example holding down
   * Search).
   *
   * @param {!AutomationEvent} evt
   */
  onMousePressed_: function(evt) {
    this.down_ = true;
    this.mouseStart_ = {x: evt.mouseX, y: evt.mouseY};
    this.startNode_ = evt.target;
    chrome.tts.stop();
    this.onMouseDragged_(evt);
  },

  /**
   * Called when the mouse is moved or dragged and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   *
   * @param {!AutomationEvent} evt
   */
  onMouseDragged_: function(evt) {
    if (!this.down_)
      return;

    var rect = rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y,
        evt.mouseX, evt.mouseY);
    chrome.accessibilityPrivate.setFocusRing([rect]);
  },

  /**
   * Called when the mouse is released and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   *
   * @param {!AutomationEvent} evt
   */
  onMouseReleased_: function(evt) {
    this.onMouseDragged_(evt);
    this.down_ = false;

    chrome.accessibilityPrivate.setFocusRing([]);

    // Walk up to the nearest window, web area, or dialog that the
    // hit node is contained inside. Only speak objects within that
    // container. In the future we might include other container-like
    // roles here.
    var root = this.startNode_;
    while (root.parent &&
        root.role != RoleType.WINDOW &&
        root.role != RoleType.ROOT_WEB_AREA &&
        root.role != RoleType.DESKTOP &&
        root.role != RoleType.DIALOG) {
      root = root.parent;
    }

    var rect = rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y,
        evt.mouseX, evt.mouseY);
    var nodes = [];
    this.findAllMatching_(root, rect, nodes);
    this.startSpeechQueue_(nodes);
  },

  /**
   * Called when the user cancels select-to-speak's capturing of mouse
   * events (for example by releasing Search while the mouse is still down).
   *
   * @param {!AutomationEvent} evt
   */
  onMouseCanceled_: function(evt) {
    this.down_ = false;
    chrome.accessibilityPrivate.setFocusRing([]);
    chrome.tts.stop();
  },

  /**
   * Finds all nodes within the subtree rooted at |node| that overlap
   * a given rectangle.
   * @param {AutomationNode} node The starting node.
   * @param {{left: number, top: number, width: number, height: number}} rect
   *     The bounding box to search.
   * @param {Array<AutomationNode>} nodes The matching node array to be
   *     populated.
   * @return {boolean} True if any matches are found.
   */
  findAllMatching_: function(node, rect, nodes) {
    var found = false;
    for (var c = node.firstChild; c; c = c.nextSibling) {
      if (this.findAllMatching_(c, rect, nodes))
        found = true;
    }

    if (found)
      return true;

    if (!node.name || !node.location)
      return false;

    if (overlaps(node.location, rect)) {
      nodes.push(node);
      return true;
    }

    return false;
  },

  /**
   * Enqueue speech commands for all of the given nodes.
   * @param {Array<AutomationNode>} nodes The nodes to speak.
   */
  startSpeechQueue_: function(nodes) {
    chrome.tts.stop();
    for (var i = 0; i < nodes.length; i++) {
      var node = nodes[i];
      var isLast = (i == nodes.length - 1);
      chrome.tts.speak(node.name || '', {
        lang: 'en-US',
        'enqueue': true,
        onEvent: (function(node, isLast, event) {
          if (event.type == 'start') {
            chrome.accessibilityPrivate.setFocusRing([node.location]);
          } else if (event.type == 'interrupted' ||
                     event.type == 'cancelled') {
            chrome.accessibilityPrivate.setFocusRing([]);
          } else if (event.type == 'end') {
            if (isLast) {
              chrome.accessibilityPrivate.setFocusRing([]);
            }
          }
        }).bind(this, node, isLast)
      });
    }
  }
};

new SelectToSpeak();
