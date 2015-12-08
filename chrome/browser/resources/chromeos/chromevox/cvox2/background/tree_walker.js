// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tree walker over the automation tree.
 */

goog.provide('AutomationTreeWalker');
goog.provide('AutomationTreeWalkerPhase');

goog.require('constants');

/**
 * Defined phases of traversal from the initial node passed to an
 * AutomationTreeWalker instance.
 * @enum {string}
 */
AutomationTreeWalkerPhase = {
  /** Walker is on the initial node. */
  INITIAL: 'initial',
  /** Walker is on an ancestor of initial node. */
  ANCESTOR: 'ancestor',
  /** Walker is on a descendant of initial node. */
  DESCENDANT: 'descendant',
  /** Walker is on a node not covered by any other phase. */
  OTHER: 'other'
};

/**
 * @param {!chrome.automation.AutomationNode} node
 * @param {constants.Dir=} opt_dir Defaults to constants.Dir.FORWARD.
 * @constructor
 */
AutomationTreeWalker = function(node, opt_dir) {
  /** @type {chrome.automation.AutomationNode} @private */
  this.node_ = node;
  /** @type {AutomationTreeWalkerPhase} @private */
  this.phase_ = AutomationTreeWalkerPhase.INITIAL;
  /** @const {constants.Dir} @private */
  this.dir_ = opt_dir ? opt_dir : constants.Dir.FORWARD;
  /** @const {!chrome.automation.AutomationNode} @private */
  this.initialNode_ = node;
  /**
   * Deepest common ancestor of initialNode and node. Valid only when moving
   * backward.
   * @type {chrome.automation.AutomationNode} @private
   */
  this.backwardAncestor_ = node.parent;
};

AutomationTreeWalker.prototype = {
  /** @type {chrome.automation.AutomationNode} */
  get node() {
    return this.node_;
  },

  /** @type {AutomationTreeWalkerPhase} */
  get phase() {
    return this.phase_;
  },

  /**
   * Moves this walker to the next node.
   * @return {!AutomationTreeWalker} The called AutomationTreeWalker, for
   *                                 chaining.
   */
  next: function() {
    if (!this.node_)
      return this;
    if (this.dir_ == constants.Dir.FORWARD)
      this.forward_(this.node_);
    else
      this.backward_(this.node_);
    return this;
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  forward_: function(node) {
    if (node.firstChild) {
      if (this.phase_ == AutomationTreeWalkerPhase.INITIAL)
        this.phase_ = AutomationTreeWalkerPhase.DESCENDANT;
      this.node_ = node.firstChild;
      return;
    }

    var searchNode = node;
    while (searchNode) {
      // We have crossed out of the initial node's subtree.
      if (searchNode == this.initialNode_)
        this.phase_ = AutomationTreeWalkerPhase.OTHER;

      if (searchNode.nextSibling) {
        this.node_ = searchNode.nextSibling;
        return;
      }
      searchNode = searchNode.parent;
    }
    this.node_ = null;
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  backward_: function(node) {
    if (node.previousSibling) {
      this.phase_ = AutomationTreeWalkerPhase.OTHER;
      node = node.previousSibling;
      while (node.lastChild)
        node = node.lastChild;
      this.node_ = node;
      return;
    }
    if (node.parent && this.backwardAncestor_ == node.parent) {
      this.phase_ = AutomationTreeWalkerPhase.ANCESTOR;
      this.backwardAncestor_ = node.parent.parent;
    }
    this.node_ = node.parent;
  }
};
