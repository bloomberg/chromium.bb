// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage interactions with the accessibility tree, including moving
 * to and selecting nodes.
 *
 * @constructor
 * @param {!chrome.automation.AutomationNode} desktop
 */
function AutomationManager(desktop) {
  /**
   * Currently highlighted node.
   *
   * @private {!chrome.automation.AutomationNode}
   */
  this.node_ = desktop;

  /**
   * The root of the subtree that the user is navigating through.
   *
   * @private {!chrome.automation.AutomationNode}
   */
  this.scope_ = desktop;

  /**
   * The desktop node.
   *
   * @private {!chrome.automation.AutomationNode}
   */
  this.desktop_ = desktop;

  /**
   * A stack of past scopes. Allows user to traverse back to previous groups
   * after selecting one or more groups. The most recent group is at the end
   * of the array.
   *
   * @private {Array<!chrome.automation.AutomationNode>}
   */
  this.scopeStack_ = [];

  /**
   * Keeps track of when we're visiting the current scope as an actionable node.
   * @private {boolean}
   */
  this.visitingScopeAsActionable_ = false;

  this.init_();
}

/**
 * Highlight colors for the focus ring to distinguish between different types
 * of nodes.
 *
 * @enum {string}
 * @const
 */
AutomationManager.Color = {
  SCOPE: '#de742f',  // dark orange
  GROUP: '#ffbb33',  // light orange
  LEAF: '#78e428'    // light green
};

AutomationManager.prototype = {
  /**
   * @private
   */
  init_: function() {
    this.desktop_.addEventListener(
        chrome.automation.EventType.FOCUS, this.handleFocusChange_.bind(this),
        false);

    // TODO(elichtenberg): Eventually use a more specific filter than
    // ALL_TREE_CHANGES.
    chrome.automation.addTreeChangeObserver(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.handleNodeRemoved_.bind(this));
  },

  /**
   * When an interesting element gains focus on the page, move to it. If an
   * element gains focus but is not interesting, move to the next interesting
   * node after it.
   *
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  handleFocusChange_: function(event) {
    if (this.node_ === event.target)
      return;

    // Rebuild scope stack and set scope for focused node.
    this.buildScopeStack_(event.target);

    // Move to focused node.
    this.node_ = event.target;

    // In case the node that gained focus is not a subtreeLeaf.
    if (SwitchAccessPredicate.isSubtreeLeaf(this.node_, this.scope_))
      this.updateFocusRing_();
    else
      this.moveForward();
  },

  /**
   * Create a new scope stack and set the current scope for |node|.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  buildScopeStack_: function(node) {
    // Create list of |node|'s ancestors, with highest level ancestor at the
    // end.
    let ancestorList = [];
    while (node.parent) {
      ancestorList.push(node.parent);
      node = node.parent;
    }

    // Starting with desktop as the scope, if an ancestor is a group, set it to
    // the new scope and push the old scope onto the scope stack.
    this.scopeStack_ = [];
    this.scope_ = this.desktop_;
    while (ancestorList.length > 0) {
      let ancestor = ancestorList.pop();
      if (ancestor.role === chrome.automation.RoleType.DESKTOP)
        continue;
      if (SwitchAccessPredicate.isGroup(ancestor, this.scope_)) {
        this.scopeStack_.push(this.scope_);
        this.scope_ = ancestor;
      }
    }
  },

  /**
   * When a node is removed from the page, move to a new valid node.
   *
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  handleNodeRemoved_: function(treeChange) {
    // TODO(elichtenberg): Only listen to NODE_REMOVED callbacks. Don't need
    // any others.
    if (treeChange.type !== chrome.automation.TreeChangeType.NODE_REMOVED)
      return;

    // TODO(elichtenberg): Currently not getting NODE_REMOVED event when whole
    // tree is deleted. Once fixed, can delete this. Should only need to check
    // if target is current node.
    let removedByRWA =
        treeChange.target.role === chrome.automation.RoleType.ROOT_WEB_AREA &&
        !this.node_.role;

    if (!removedByRWA && treeChange.target !== this.node_)
      return;

    chrome.accessibilityPrivate.setFocusRing([]);

    // Current node not invalid until after treeChange callback, so move to
    // valid node after callback. Delay added to prevent moving to another
    // node about to be made invalid. If already at a valid node (e.g., user
    // moves to it or focus changes to it), won't need to move to a new node.
    window.setTimeout(function() {
      if (!this.node_.role)
        this.moveForward();
    }.bind(this), 100);
  },

  /**
   * Find the next interesting node, and update |this.node_|. If there is no
   * next node, |this.node_| will be set equal to |this.scope_| to loop again.
   */
  moveForward: function() {
    // If node is invalid, set node to last valid scope.
    this.startAtValidNode_();

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.scope_));

    // Special case: Scope is actionable.
    if (this.node_ === this.scope_ &&
        SwitchAccessPredicate.isActionable(this.node_) &&
        !this.visitingScopeAsActionable_) {
      this.showScopeAsActionable_();
      return;
    }
    this.visitingScopeAsActionable_ = false;

    let node = treeWalker.next().node;
    // If treeWalker returns undefined, that means we're at the end of the tree
    // and we should start over.
    if (!node)
      node = this.scope_;

    this.setCurrentNode_(node);
  },

  /**
   * Find the previous interesting node and update |this.node_|. If there is no
   * previous node, |this.node_| will be set to the youngest descendant in the
   * SwitchAccess scope tree to loop again.
   */
  moveBackward: function() {
    // If node is invalid, set node to last valid scope.
    this.startAtValidNode_();

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.scope_));

    // Special case: Scope is actionable
    if (this.node_ === this.scope_ && this.visitingScopeAsActionable_) {
      this.visitingScopeAsActionable_ = false;
      this.setCurrentNode_(this.node_);
      return;
    }

    let node = treeWalker.next().node;

    // Special case: Scope is actionable
    if (node === this.scope_ && SwitchAccessPredicate.isActionable(node)) {
      this.showScopeAsActionable_();
      return;
    }

    // If treeWalker returns undefined, that means we're at the end of the tree
    // and we should start over.
    if (!node)
      node = this.youngestDescendant_(this.scope_);

    this.setCurrentNode_(node);
  },

  /**
   * Set |this.node_| to |node|, and update its appearance onscreen.
   *
   * @param {!chrome.automation.AutomationNode} node
   */
  setCurrentNode_: function(node) {
    this.node_ = node;
    this.updateFocusRing_();
  },

  /**
   * Show the current scope as an actionable item.
   */
  showScopeAsActionable_: function() {
    this.node_ = this.scope_;
    this.visitingScopeAsActionable_ = true;

    this.updateFocusRing_(AutomationManager.Color.LEAF);
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
    if (!this.node_.role)
      return;

    if (this.node_ === this.scope_) {
      // If we're visiting the scope as actionable, perform the default action.
      if (this.visitingScopeAsActionable_) {
        this.node_.doDefault();
        return;
      }

      // Don't let user select the top-level root node (i.e., the desktop node).
      if (this.scopeStack_.length === 0)
        return;

      // Find a previous scope that is still valid. The stack here always has
      // at least one valid scope (i.e., the desktop node).
      do {
        this.scope_ = this.scopeStack_.pop();
      } while (!this.scope_.role && this.scopeStack_.length > 0);

      this.updateFocusRing_();
      return;
    }

    if (SwitchAccessPredicate.isGroup(this.node_, this.scope_)) {
      this.scopeStack_.push(this.scope_);
      this.scope_ = this.node_;
      this.moveForward();
      return;
    }

    this.node_.doDefault();
  },

  /**
   * Set the focus ring for the current node and determine the color for it.
   *
   * @param {AutomationManager.Color=} opt_color
   * @private
   */
  updateFocusRing_: function(opt_color) {
    let color;
    if (this.node_ === this.scope_)
      color = AutomationManager.Color.SCOPE;
    else if (SwitchAccessPredicate.isGroup(this.node_, this.scope_))
      color = AutomationManager.Color.GROUP;
    else
      color = AutomationManager.Color.LEAF;

    color = opt_color || color;
    chrome.accessibilityPrivate.setFocusRing([this.node_.location], color);
  },

  /**
   * Checks if this.node_ is valid. If so, do nothing.
   *
   * If this.node_ is not valid, set this.node_ to a valid scope. Will check the
   * current scope and past scopes until a valid scope is found. this.node_
   * is set to that valid scope.
   *
   * @private
   */
  startAtValidNode_: function() {
    if (this.node_.role)
      return;

    // Current node is invalid, but current scope is still valid, so set node
    // to the current scope.
    if (this.scope_.role)
      this.node_ = this.scope_;

    // Current node and current scope are invalid, so set both to a valid scope
    // from the scope stack. The stack here always has at least one valid scope
    // (i.e., the desktop node).
    while (!this.node_.role && this.scopeStack_.length > 0) {
      this.node_ = this.scopeStack_.pop();
      this.scope_ = this.node_;
    }
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
            'At index ' + node.indexInParent + ', there are ' + numSiblings +
            ' siblings');
      }
      console.log('Has ' + node.children.length + ' children');
    } else {
      console.log('Node is null');
    }
    console.log(node);
    console.log('\n');
  },

  /**
   * Get the youngest descendant of |node|, if it has one within the current
   * scope.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode}
   * @private
   */
  youngestDescendant_: function(node) {
    let leaf = SwitchAccessPredicate.leaf(this.scope_);

    while (node.lastChild && !leaf(node))
      node = node.lastChild;

    return node;
  }
};
