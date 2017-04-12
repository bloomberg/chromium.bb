// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to move to the appropriate node in the accessibility tree.
 *
 * @constructor
 */
function AutomationTreeWalker() {};

AutomationTreeWalker.prototype = {
  /**
   * Return the next/previous interesting node from |start|. If no interesting
   * node is found, return the first/last interesting node. If |doNext| is true,
   * will search for next node. Otherwise, will search for previous node.
   *
   * @param {chrome.automation.AutomationNode} start
   * @param {chrome.automation.AutomationNode} root
   * @param {boolean} doNext
   * @return {chrome.automation.AutomationNode}
   */
  moveToNode: function(start, root, doNext) {
    let node = start;
    if (node) {
      do {
        node = doNext ? this.getNextNode_(node) : this.getPreviousNode_(node);
      } while (node && !this.isInteresting_(node));
      if (node)
        return node;
    }

    if (root) {
      console.log(
          'Restarting search for node at ' + (doNext ? 'first' : 'last'));
      node = doNext ? root.firstChild : this.getYoungestDescendant_(root);
      while (node && !this.isInteresting_(node))
        node = doNext ? this.getNextNode_(node) : this.getPreviousNode_(node);
      if (node)
        return node;
    }

    console.log('Found no interesting nodes to visit.');
    return null;
  },

  /**
   * Given a flat list of nodes in pre-order, get the node that comes after
   * |node|.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode|undefined}
   * @private
   */
  getNextNode_: function(node) {
    // Check for child.
    let child = node.firstChild;
    if (child)
      return child;

    // No child. Check for right-sibling.
    let sibling = node.nextSibling;
    if (sibling)
      return sibling;

    // No right-sibling. Get right-sibling of closest ancestor.
    let ancestor = node.parent;
    while (ancestor) {
      let aunt = ancestor.nextSibling;
      if (aunt)
        return aunt;
      ancestor = ancestor.parent;
    }

    // No node found after |node|, so return undefined.
    return undefined;
  },

  /**
   * Given a flat list of nodes in pre-order, get the node that comes before
   * |node|.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode|undefined}
   * @private
   */
  getPreviousNode_: function(node) {
    // Check for left-sibling. If a left-sibling exists, return its youngest
    // descendant if it has one, or otherwise return the sibling.
    let sibling = node.previousSibling;
    if (sibling)
      return this.getYoungestDescendant_(sibling) || sibling;

    // No left-sibling. Return parent if it exists; otherwise return undefined.
    return node.parent;
  },

  /**
   * Get the youngest descendant of |node| if it has one.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode|undefined}
   * @private
   */
  getYoungestDescendant_: function(node) {
    if (!node.lastChild)
      return undefined;

    while (node.lastChild)
      node = node.lastChild;

    return node;
  },

  /**
   * Returns true if |node| is interesting.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   * @private
   */
  isInteresting_: function(node) {
    let loc = node.location;
    let parent = node.parent;
    let root = node.root;
    let role = node.role;
    let state = node.state;

    // Skip things that are offscreen
    if (state[chrome.automation.StateType.OFFSCREEN]
        || loc.top < 0 || loc.left < 0)
      return false;

    if (parent) {
      // crbug.com/710559
      // Work around for browser tabs
      if (role === chrome.automation.RoleType.TAB
          && parent.role === chrome.automation.RoleType.TAB_LIST
          && root.role === chrome.automation.RoleType.DESKTOP)
        return true;
    }

    // The general rule that applies to everything.
    return state[chrome.automation.StateType.FOCUSABLE] === true;
  },

  /**
   * Return the next sibling of |node| if it has one.
   *
   * @param {chrome.automation.AutomationNode} node
   * @return {chrome.automation.AutomationNode}
   */
  debugMoveToNext: function(node) {
    if (!node)
      return null;

    let next = node.nextSibling;
    if (next) {
      return next;
    } else {
      console.log('Node is last of siblings');
      console.log('\n');
      return null;
    }
  },

  /**
   * Return the previous sibling of |node| if it has one.
   *
   * @param {chrome.automation.AutomationNode} node
   * @return {chrome.automation.AutomationNode}
   */
  debugMoveToPrevious: function(node) {
    if (!node)
      return null;

    let prev = node.previousSibling;
    if (prev) {
      return prev;
    } else {
      console.log('Node is first of siblings');
      console.log('\n');
      return null;
    }
  },

  /**
   * Return the first child of |node| if it has one.
   *
   * @param {chrome.automation.AutomationNode} node
   * @return {chrome.automation.AutomationNode}
   */
  debugMoveToFirstChild: function(node) {
    if (!node)
      return null;

    let child = node.firstChild;
    if (child) {
      return child;
    } else {
      console.log('Node has no children');
      console.log('\n');
      return null;
    }
  },

  /**
   * Return the parent of |node| if it has one.
   *
   * @param {chrome.automation.AutomationNode} node
   * @return {chrome.automation.AutomationNode}
   */
  debugMoveToParent: function(node) {
    if (!node)
      return null;

    let parent = node.parent;
    if (parent) {
      return parent;
    } else {
      console.log('Node has no parent');
      console.log('\n');
      return null;
    }
  }
};
