// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage interactions with the accessibility tree, including moving
 * to and selecting nodes.
 *
 * @constructor
 */
function AutomationManager() {
  /**
   * Currently selected node.
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.node_ = null;

  /**
   * Root node (i.e., the desktop).
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.root_ = null;

  /**
   * Moves to the appropriate node in the accessibility tree.
   *
   * @private {AutomationTreeWalker}
   */
  this.treeWalker_ = null;

  this.init_();
};

AutomationManager.prototype = {
  /**
   * Set this.node_ and this.root_ to the desktop node, and initialize the
   * tree walker.
   *
   * @private
   */
  init_: function() {
    this.treeWalker_ = new AutomationTreeWalker();

    chrome.automation.getDesktop(function(desktop) {
      this.node_ = desktop;
      this.root_ = desktop;
      console.log('AutomationNode for desktop is loaded');
      this.printNode_(this.node_);
    }.bind(this));
  },

  /**
   * Set this.node_ to the next/previous interesting node, and then highlight
   * it on the screen. If no interesting node is found, set this.node_ to the
   * first/last interesting node. If |doNext| is true, will search for next
   * node. Otherwise, will search for previous node.
   *
   * @param {boolean} doNext
   */
  moveToNode: function(doNext) {
    let node = this.treeWalker_.moveToNode(this.node_, this.root_, doNext);
    if (node) {
      this.node_ = node;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Perform the default action on the currently selected node.
   */
  doDefault: function() {
    if (!this.node_)
      return;

    this.node_.doDefault();
  },

  // TODO(elichtenberg): Move print functions to a custom logger class. Only
  // log when debuggingEnabled is true.
  /**
   * Print out details about a node.
   *
   * @param {chrome.automation.AutomationNode} node
   * @private
   */
  printNode_: function(node) {
    if (node) {
      console.log('Name = ' + node.name);
      console.log('Role = ' + node.role);
      console.log('Root role = ' + node.root.role);
      if (!node.parent)
        console.log('At index ' + node.indexInParent + ', has no parent');
      else {
        let numSiblings = node.parent.children.length;
        console.log(
            'At index ' + node.indexInParent + ', there are '
            + numSiblings + ' siblings');
      }
      console.log('Has ' + node.children.length + ' children');
    } else {
      console.log('Node is null');
    }
    console.log(node);
    console.log('\n');
  },

  /**
   * Move to the next sibling of this.node_ if it has one.
   */
  debugMoveToNext: function() {
    let next = this.treeWalker_.debugMoveToNext(this.node_);
    if (next) {
      this.node_ = next;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the previous sibling of this.node_ if it has one.
   */
  debugMoveToPrevious: function() {
    let prev = this.treeWalker_.debugMoveToPrevious(this.node_);
    if (prev) {
      this.node_ = prev;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the first child of this.node_ if it has one.
   */
  debugMoveToFirstChild: function() {
    let child = this.treeWalker_.debugMoveToFirstChild(this.node_);
    if (child) {
      this.node_ = child;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  },

  /**
   * Move to the parent of this.node_ if it has one.
   */
  debugMoveToParent: function() {
    let parent = this.treeWalker_.debugMoveToParent(this.node_);
    if (parent) {
      this.node_ = parent;
      this.printNode_(this.node_);
      chrome.accessibilityPrivate.setFocusRing([this.node_.location]);
    }
  }
};
