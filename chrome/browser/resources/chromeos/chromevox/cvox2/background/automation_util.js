// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox utilities for the automation extension API.
 */

goog.provide('AutomationUtil');

goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('constants');

/**
 * @constructor
 */
AutomationUtil = function() {};

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var RoleType = chrome.automation.RoleType;

/**
 * Find a node in subtree of |cur| satisfying |pred| using pre-order traversal.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {Dir} dir
 * @param {AutomationPredicate.Unary} pred A predicate to apply
 *     to a candidate node.
 * @return {AutomationNode}
 */
AutomationUtil.findNodePre = function(cur, dir, pred) {
  if (!cur)
    return null;

  if (pred(cur))
    return cur;

  var child = dir == Dir.BACKWARD ? cur.lastChild : cur.firstChild;
  while (child) {
    var ret = AutomationUtil.findNodePre(child, dir, pred);
    if (ret)
      return ret;
    child = dir == Dir.BACKWARD ?
        child.previousSibling : child.nextSibling;
  }
};

/**
 * Find a node in subtree of |cur| satisfying |pred| using post-order traversal.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {Dir} dir
 * @param {AutomationPredicate.Unary} pred A predicate to apply
 *     to a candidate node.
 * @return {AutomationNode}
 */
AutomationUtil.findNodePost = function(cur, dir, pred) {
  if (!cur)
    return null;

  var child = dir == Dir.BACKWARD ? cur.lastChild : cur.firstChild;
  while (child) {
    var ret = AutomationUtil.findNodePost(child, dir, pred);
    if (ret)
      return ret;
    child = dir == Dir.BACKWARD ?
        child.previousSibling : child.nextSibling;
  }

  if (pred(cur))
    return cur;
};

/**
 * Find the next node in the given direction in depth first order.
 * @param {!AutomationNode} cur Node to begin the search from.
 * @param {Dir} dir
 * @param {AutomationPredicate.Unary} pred A predicate to apply
 *     to a candidate node.
 * @param {AutomationTreeWalkerRestriction=} opt_restrictions |leaf|, |root|,
 *     and |skipInitialSubtree| are valid restrictions used when finding the
 *     next node. If not supplied, the leaf predicate returns true for nodes
 *     matched by |pred| or |AutomationPredicate.container|. This is typically
 *     desirable in most situations.
 *     If not supplied, the root predicate gets set to
 *     |AutomationUtil.isTraversalRoot|.
 * @return {AutomationNode}
 */
AutomationUtil.findNextNode = function(cur, dir, pred, opt_restrictions) {
  var restrictions = {};
  opt_restrictions = opt_restrictions || {leaf: undefined,
                                          root: undefined,
                                          visit: undefined,
                                          skipInitialSubtree: false};
  restrictions.leaf = opt_restrictions.leaf || function(node) {
    // Treat nodes matched by |pred| as leaves except for containers.
    return !AutomationPredicate.container(node) && pred(node);
  };

  restrictions.root = opt_restrictions.root || AutomationUtil.isTraversalRoot;
  restrictions.skipInitialSubtree = opt_restrictions.skipInitialSubtree;

  restrictions.visit = function(node) {
    if (pred(node) && !AutomationPredicate.shouldIgnoreLeaf(node))
      return true;
    if (AutomationPredicate.container(node))
      return true;
  };

  var walker = new AutomationTreeWalker(cur, dir, restrictions);
  return walker.next().node;
};

/**
 * Given nodes a_1, ..., a_n starting at |cur| in pre order traversal, apply
 * |pred| to a_i and a_(i - 1) until |pred| is satisfied.  Returns a_(i - 1) or
 * a_i (depending on opt_before) or null if no match was found.
 * @param {!AutomationNode} cur
 * @param {Dir} dir
 * @param {AutomationPredicate.Binary} pred
 * @param {boolean=} opt_before True to return a_(i - 1); a_i otherwise.
 *                              Defaults to false.
 * @return {AutomationNode}
 */
AutomationUtil.findNodeUntil = function(cur, dir, pred, opt_before) {
  var before = cur;
  var after = before;
  do {
    before = after;
    after = AutomationUtil.findNextNode(before, dir, AutomationPredicate.leaf);
  } while (after && !pred(before, after));
  return opt_before ? before : after;
};

/**
 * Returns an array containing ancestors of node starting at root down to node.
 * @param {!AutomationNode} node
 * @return {!Array<AutomationNode>}
 */
AutomationUtil.getAncestors = function(node) {
  var ret = [];
  var candidate = node;
  while (candidate) {
    ret.push(candidate);

    if (!AutomationUtil.isInSameTree(candidate, candidate.parent))
      break;

    candidate = candidate.parent;
  }
  return ret.reverse();
};

/**
 * Gets the first index where the two input arrays differ. Returns -1 if they
 * do not.
 * @param {!Array<AutomationNode>} ancestorsA
 * @param {!Array<AutomationNode>} ancestorsB
 * @return {number}
 */
AutomationUtil.getDivergence = function(ancestorsA, ancestorsB) {
  for (var i = 0; i < ancestorsA.length; i++) {
    if (ancestorsA[i] !== ancestorsB[i])
      return i;
  }
  if (ancestorsA.length == ancestorsB.length)
    return -1;
  return ancestorsA.length;
};

/**
 * Returns ancestors of |node| that are not also ancestors of |prevNode|.
 * @param {!AutomationNode} prevNode
 * @param {!AutomationNode} node
 * @return {!Array<AutomationNode>}
 */
AutomationUtil.getUniqueAncestors = function(prevNode, node) {
  var prevAncestors = AutomationUtil.getAncestors(prevNode);
  var ancestors = AutomationUtil.getAncestors(node);
  var divergence = AutomationUtil.getDivergence(prevAncestors, ancestors);
  return ancestors.slice(divergence);
};

/**
 * Given |nodeA| and |nodeB| in that order, determines their ordering in the
 * document.
 * @param {!AutomationNode} nodeA
 * @param {!AutomationNode} nodeB
 * @return {Dir}
 */
AutomationUtil.getDirection = function(nodeA, nodeB) {
  var ancestorsA = AutomationUtil.getAncestors(nodeA);
  var ancestorsB = AutomationUtil.getAncestors(nodeB);
  var divergence = AutomationUtil.getDivergence(ancestorsA, ancestorsB);

  // Default to Dir.FORWARD.
  if (divergence == -1)
    return Dir.FORWARD;

  var divA = ancestorsA[divergence];
  var divB = ancestorsB[divergence];

  // One of the nodes is an ancestor of the other. Don't distinguish and just
  // consider it Dir.FORWARD.
  if (!divA || !divB || divA.parent === nodeB || divB.parent === nodeA)
    return Dir.FORWARD;

  return divA.indexInParent <= divB.indexInParent ? Dir.FORWARD : Dir.BACKWARD;
};

/**
 * Determines whether the two given nodes come from the same tree source.
 * @param {AutomationNode} a
 * @param {AutomationNode} b
 * @return {boolean}
 */
AutomationUtil.isInSameTree = function(a, b) {
  if (!a || !b)
    return true;

  // Given two non-desktop roots, consider them in the "same" tree.
  return a.root === b.root ||
      (a.root.role == b.root.role && a.root.role == RoleType.rootWebArea);
};

/**
 * Returns whether the given node should not be crossed when performing
 * traversals up the ancestry chain.
 * @param {AutomationNode} node
 * @return {boolean}
 */
AutomationUtil.isTraversalRoot = function(node) {
  switch (node.role) {
    case RoleType.dialog:
    case RoleType.window:
      return true;
    case RoleType.toolbar:
      return node.root.role == RoleType.desktop;
    case RoleType.rootWebArea:
      return !node.parent || node.parent.root.role == RoleType.desktop;
    default:
      return false;
  }
};

/**
 * Determines whether the two given nodes come from the same webpage.
 * @param {AutomationNode} a
 * @param {AutomationNode} b
 * @return {boolean}
 */
AutomationUtil.isInSameWebpage = function(a, b) {
  if (!a || !b)
    return false;

  a = a.root;
  while (a && a.parent && AutomationUtil.isInSameTree(a.parent, a))
    a = a.parent.root;

  b = b.root;
  while (b && b.parent && AutomationUtil.isInSameTree(b.parent, b))
    b = b.parent.root;

  return a == b;
};

});  // goog.scope
