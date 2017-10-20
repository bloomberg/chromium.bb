// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationNode = chrome.automation.AutomationNode;
var RoleType = chrome.automation.RoleType;

/**
 * Gets the first ancestor of a node which is a paragraph, or display:block,
 * or the root node if none is found.
 * @param { AutomationNode } node The node to get the parent for.
 * @return { ?AutomationNode } the parent paragraph or null if there is none.
 */
function getFirstBlockAncestor(node) {
  let parent = node.parent;
  let root = node.root;
  while (parent != null) {
    if (parent == root) {
      return parent;
    }
    if (parent.role == RoleType.PARAGRAPH) {
      return parent;
    }
    if ((parent.display == 'block' || parent.display == 'inline-block') &&
        parent.role != RoleType.STATIC_TEXT) {
      return parent;
    }
    parent = parent.parent;
  }
  return null;
}

/**
 * Determines whether two nodes are in the same block-like ancestor, i.e.
 * whether they are in the same paragraph.
 * @param { AutomationNode|undefined } first The first node to compare.
 * @param { AutomationNode|undefined } second The second node to compare.
 * @return { boolean } whether two nodes are in the same paragraph.
 */
function inSameParagraph(first, second) {
  if (first === undefined || second === undefined) {
    return false;
  }
  // TODO: Clean up this check after crbug.com/774308 is resolved.
  // At that point we will only need to check for display:block or inline-block.
  if (((first.display == 'block' || first.display == 'inline-block') &&
       first.role != RoleType.STATIC_TEXT &&
       first.role != RoleType.INLINE_TEXT_BOX) ||
      ((second.display == 'block' || second.display == 'inline-block') &&
       second.role != RoleType.STATIC_TEXT &&
       second.role != RoleType.INLINE_TEXT_BOX)) {
    // 'block' or 'inline-block' elements cannot be in the same paragraph.
    return false;
  }
  let firstBlock = getFirstBlockAncestor(first);
  let secondBlock = getFirstBlockAncestor(second);
  return firstBlock != undefined && firstBlock == secondBlock;
}

/**
 * Determines whether a string is only whitespace.
 * @param { string } name A string to test
 * @return { boolean } whether the string is only whitespace
 */
function isWhitespace(name) {
  if (name.length == 0) {
    return true;
  }
  // Search for one or more whitespace characters
  let re = /^\s+$/;
  return re.exec(name) != null;
}

/**
 * Builds information about nodes in a group until it reaches the end of the
 * group. It may return a NodeGroup with a single node, or a large group
 * representing a paragraph of inline nodes.
 * @param { Array<AutomationNode> } nodes List of automation nodes to use.
 * @param { number } index The index into nodes at which to start.
 * @return { NodeGroup } info about the node group
 */
function buildNodeGroup(nodes, index) {
  let node = nodes[index];
  let next = nodes[index + 1];
  let result = new NodeGroup(index);
  // TODO: Don't skip nodes. Instead, go through every node in
  // this paragraph from the first to the last in the nodes list.
  // This will catch nodes at the edges of the user's selection like
  // short links at the beginning or ends of sentences.
  //
  // While next node is in the same paragraph as this node AND is
  // a text type node, continue building the paragraph.
  while (index < nodes.length) {
    if (node.name !== undefined && !isWhitespace(node.name)) {
      result.nodes.push(new NodeGroupItem(node, result.text.length));
      result.text += node.name + ' ';
    }
    if (!inSameParagraph(node, next)) {
      break;
    }
    index += 1;
    node = next;
    next = nodes[index + 1];
  }
  result.endIndex = index;
  return result;
}

/**
 * Class representing a node group, which may be a single node or a
 * full paragraph of nodes.
 *
 * @param { number } startIndex The index of the first node within
 * @constructor
 */
function NodeGroup(startIndex) {
  /**
   * Full text of this paragraph.
   * @type { string|undefined }
   */
  this.text = '';

  /**
   * List of nodes in this paragraph in order.
   * @type { Array<NodeGroupItem> }
   */
  this.nodes = [];

  /**
   * The index of the first node in this paragraph from the list of
   * nodes originally selected by the user.
   * @type { number }
   */
  this.startIndex = startIndex;

  /**
   * The index of the last node in this paragraph from the list of
   * nodes originally selected by the user.
   * @type { number }
   */
  this.endIndex = -1;
}

/**
 * Class representing an automation node within a block of text, like
 * a paragraph. Each Item in a NodeGroup has a start index within the
 * total text, as well as the original AutomationNode it was associated
 * with.
 *
 * @param { AutomationNode } node The AutomationNode associated with this item
 * @param { number } startChar The index into the NodeGroup's text string where
 *                             this item begins.
 * @constructor
 */
function NodeGroupItem(node, startChar) {
  /**
   * @type { AutomationNode }
   */
  this.node = node;

  /**
   * The index into the NodeGroup's text string that is the first character
   * of the text of this automation node.
   * @type { number }
   */
  this.startChar = startChar;
}
