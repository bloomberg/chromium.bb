// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for automation nodes in Select-to-Speak.

/**
 * Node state. Nodes can be on-screen like normal, or they may
 * be invisible if they are in a tab that is not in the foreground
 * or similar, or they may be invalid if they were removed from their
 * root, i.e. if they were in a window that was closed.
 * @enum {number}
 */
const NodeState = {
  NODE_STATE_INVALID: 0,
  NODE_STATE_INVISIBLE: 1,
  NODE_STATE_NORMAL: 2,
};

/**
 * Gets the current visiblity state for a given node.
 *
 * @param {AutomationNode} node The starting node.
 * @return {NodeState} the current node state.
 */
function getNodeState(node) {
  if (node === undefined || node.root === null || node.root === undefined) {
    // The node has been removed from the tree, perhaps because the
    // window was closed.
    return NodeState.NODE_STATE_INVALID;
  }
  // This might not be populated correctly on children nodes even if their
  // parents or roots are now invisible.
  // TODO: Update the C++ bindings to set 'invisible' automatically based
  // on parents, rather than going through parents in JS below.
  if (node.state.invisible) {
    return NodeState.NODE_STATE_INVISIBLE;
  }
  // Walk up the tree to make sure the window it is in is not invisible.
  var window = getNearestContainingWindow(node);
  if (window != null && window.state[chrome.automation.StateType.INVISIBLE]) {
    return NodeState.NODE_STATE_INVISIBLE;
  }
  // TODO: Also need a check for whether the window is minimized,
  // which would also return NodeState.NODE_STATE_INVISIBLE.
  return NodeState.NODE_STATE_NORMAL;
}

/**
 * Returns true if a node should be ignored by Select-to-Speak.
 * @param {AutomationNode} node The node to test
 * @param {boolean} includeOffscreen Whether to include offscreen nodes.
 * @return {boolean} whether this node should be ignored.
 */
function shouldIgnoreNode(node, includeOffscreen) {
  return (
      !node.name || isWhitespace(node.name) || !node.location ||
      node.state.invisible || (node.state.offscreen && !includeOffscreen));
}

/**
 * Gets the first window containing this node.
 */
function getNearestContainingWindow(node) {
  // Go upwards to root nodes' parents until we find the first window.
  if (node.root.role == RoleType.ROOT_WEB_AREA) {
    var nextRootParent = node;
    while (nextRootParent != null && nextRootParent.role != RoleType.WINDOW &&
           nextRootParent.root != null &&
           nextRootParent.root.role == RoleType.ROOT_WEB_AREA) {
      nextRootParent = nextRootParent.root.parent;
    }
    return nextRootParent;
  }
  // If the parent isn't a root web area, just walk up the tree to find the
  // nearest window.
  var parent = node;
  while (parent != null && parent.role != chrome.automation.RoleType.WINDOW) {
    parent = parent.parent;
  }
  return parent;
}

/**
 * Gets the length of a node's name. Returns 0 if the name is
 * undefined.
 * @param {AutomationNode} node The node for which to check the name.
 * @return {number} The length of the node's name
 */
function nameLength(node) {
  return node.name ? node.name.length : 0;
}

/**
 * Gets the first (left-most) leaf node of a node. Returns undefined if
 *  none is found.
 * @param {AutomationNode} node The node to search for the first leaf.
 * @return {AutomationNode|undefined} The leaf node.
 */
function getFirstLeafChild(node) {
  let result = node.firstChild;
  while (result && result.firstChild) {
    result = result.firstChild;
  }
  return result;
}

/**
 * Gets the first (left-most) leaf node of a node. Returns undefined
 * if none is found.
 * @param {AutomationNode} node The node to search for the first leaf.
 * @return {AutomationNode|undefined} The leaf node.
 */
function getLastLeafChild(node) {
  let result = node.lastChild;
  while (result && result.lastChild) {
    result = result.lastChild;
  }
  return result;
}

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
function findAllMatching(node, rect, nodes) {
  var found = false;
  for (var c = node.firstChild; c; c = c.nextSibling) {
    if (findAllMatching(c, rect, nodes))
      found = true;
  }

  if (found)
    return true;

  // Closure needs node.location check here to allow the next few
  // lines to compile.
  if (shouldIgnoreNode(node, /* don't include offscreen */ false) ||
      node.location === undefined)
    return false;

  if (overlaps(node.location, rect)) {
    if (!node.children || node.children.length == 0 ||
        node.children[0].role != RoleType.INLINE_TEXT_BOX) {
      // Only add a node if it has no inlineTextBox children. If
      // it has text children, they will be more precisely bounded
      // and specific, so no need to add the parent node.
      nodes.push(node);
      return true;
    }
  }

  return false;
}

/**
 * Class representing a position on the accessibility, made of a
 * selected node and the offset of that selection.
 * @typedef {{node: (!AutomationNode),
 *            offset: (number)}}
 */
var Position;

/**
 * Finds the deep equivalent node where a selection starts given a node
 * object and selection offset. This is meant to be used in conjunction with
 * the anchorObject/anchorOffset and focusObject/focusOffset of the
 * automation API.
 * @param {AutomationNode} parent The parent node of the selection,
 * similar to chrome.automation.focusObject.
 * @param {number} offset The integer offset of the selection. This is
 * similar to chrome.automation.focusOffset.
 * @return {!Position} The node matching the selected offset.
 */
function getDeepEquivalentForSelection(parent, offset) {
  if (parent.children.length == 0)
    return {node: parent, offset: offset};
  // Create a stack of children nodes to search through.
  let nodesToCheck = parent.children.slice().reverse();
  let index = 0;
  var node;
  // Delve down into the children recursively to find the
  // one at this offset.
  while (nodesToCheck.length > 0) {
    node = nodesToCheck.pop();
    if (node.children.length > 0) {
      nodesToCheck = nodesToCheck.concat(node.children.slice().reverse());
    } else {
      index += node.name ? node.name.length : 0;
      if (index > offset) {
        return {node: node, offset: offset - index + node.name.length};
      }
    }
  }
  // We are off the end of the last node.
  return {node: node, offset: node.name ? node.name.length : 0};
}
