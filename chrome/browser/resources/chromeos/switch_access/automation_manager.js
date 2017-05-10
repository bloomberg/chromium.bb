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
   * Currently highlighted node.
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.node_ = null;

  /**
   * The root of the subtree that the user is navigating through.
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.scope_ = null;

  /**
   * The desktop node.
   *
   * @private {chrome.automation.AutomationNode}
   */
  this.desktop_ = null;

  /**
   * A stack of past scopes. Allows user to traverse back to previous groups
   * after selecting one or more groups. The most recent group is at the end
   * of the array.
   *
   * @private {Array<chrome.automation.AutomationNode>}
   */
  this.scopeStack_ = [];

  /**
   * Moves to the appropriate node in the accessibility tree.
   *
   * @private {AutomationTreeWalker}
   */
  this.treeWalker_ = null;

  this.init_();
};

/**
 * Highlight colors for the focus ring to distinguish between different types
 * of nodes.
 *
 * @const
 */
AutomationManager.Color = {
  SCOPE: '#de742f', // dark orange
  GROUP: '#ffbb33', // light orange
  LEAF: '#78e428' //light green
};

AutomationManager.prototype = {
  /**
   * Set this.node_, this.root_, and this.desktop_ to the desktop node, and
   * creates an initial tree walker.
   *
   * @private
   */
  init_: function() {
    chrome.automation.getDesktop(function(desktop) {
      this.node_ = desktop;
      this.scope_ = desktop;
      this.desktop_ = desktop;
      this.treeWalker_ = this.createTreeWalker_(desktop);
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
    if (!this.treeWalker_)
      return;

    let node = this.treeWalker_.moveToNode(doNext);
    if (node) {
      this.node_ = node;
      this.printNode_(this.node_);

      let color;
      if (this.node_ === this.scope_)
        color = AutomationManager.Color.SCOPE;
      else if (AutomationPredicate.isInteresting(this.node_))
        color = AutomationManager.Color.LEAF;
      else
        color = AutomationManager.Color.GROUP;
      chrome.accessibilityPrivate.setFocusRing([this.node_.location], color);
    }
  },

  /**
   * Select the currently highlighted node. If the node is the current scope,
   * go back to the previous scope (i.e., create a new tree walker rooted at
   * the previous scope). If the node is a group other than the current scope,
   * create a new tree walker for the new subtree the user is scanning through.
   * Otherwise, meaning the node is interesting, perform the default action on
   * it.
   */
  selectCurrentNode: function() {
    if (!this.node_ || !this.scope_ || !this.treeWalker_)
      return;

    if (this.node_ === this.scope_) {
      // Don't let user select the top-level root node (i.e., the desktop node).
      if (this.scopeStack_.length === 0)
        return;

      // Find a previous scope that is still valid.
      let oldScope;
      do {
        oldScope = this.scopeStack_.pop();
      } while (oldScope && !oldScope.role);

      // oldScope will always be valid here, so this will always be true.
      if (oldScope) {
        this.scope_ = oldScope;
        this.treeWalker_ = this.createTreeWalker_(this.scope_, this.node_);
        chrome.accessibilityPrivate.setFocusRing(
            [this.node_.location], AutomationManager.Color.GROUP);
      }
      return;
    }

    if (AutomationPredicate.isGroup(this.node_, this.scope_)) {
      this.scopeStack_.push(this.scope_);
      this.scope_ = this.node_;
      this.treeWalker_ = this.createTreeWalker_(this.scope_);
      this.moveToNode(true);
      return;
    }

    this.node_.doDefault();
  },

  /**
   * Create an AutomationTreeWalker for the subtree with |scope| as its root.
   * If |opt_start| is defined, the tree walker will start walking the tree
   * from |opt_start|; otherwise, it will start from |scope|.
   *
   * @param {!chrome.automation.AutomationNode} scope
   * @param {!chrome.automation.AutomationNode=} opt_start
   * @return {!AutomationTreeWalker}
   */
  createTreeWalker_: function(scope, opt_start) {
    // If no explicit start node, start walking the tree from |scope|.
    let start = opt_start || scope;

    let leafPred = function(node) {
      return (node !== scope && AutomationPredicate.isSubtreeLeaf(node, scope))
          || !AutomationPredicate.isInterestingSubtree(node);
    };
    let visitPred = function(node) {
      // Avoid visiting the top-level root node (i.e., the desktop node).
      return node !== this.desktop_
          && AutomationPredicate.isSubtreeLeaf(node, scope);
    }.bind(this);

    let restrictions = {
      leaf: leafPred,
      visit: visitPred
    };
    return new AutomationTreeWalker(start, scope, restrictions);
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
