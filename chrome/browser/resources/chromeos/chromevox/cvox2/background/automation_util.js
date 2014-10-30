// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox utilities for the automation extension API.
 */

goog.provide('AutomationUtil');
goog.provide('AutomationUtil.Dir');

goog.require('AutomationPredicate');

/**
 * @constructor
 */
AutomationUtil = function() {};

/**
 * Possible directions to perform tree traversals.
 * @enum {string}
 */
AutomationUtil.Dir = {
  // Search from left to right.
  FORWARD: 'forward',

  // Search from right to left.
  BACKWARD: 'backward'
};


goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = AutomationUtil.Dir;

/**
 * Find a node in subtree of |cur| satisfying |pred| using pre-order traversal.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {Dir} dir
 * @param {AutomationPredicate.Unary} pred A predicate to apply
 *     to a candidate node.
 * @return {AutomationNode}
 */
AutomationUtil.findNodePre = function(cur, dir, pred) {
  if (pred(cur))
    return cur;

  var child = dir == Dir.BACKWARD ? cur.lastChild() : cur.firstChild();
  while (child) {
    var ret = AutomationUtil.findNodePre(child, dir, pred);
    if (ret)
      return ret;
    child = dir == Dir.BACKWARD ?
        child.previousSibling() : child.nextSibling();
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
  var child = dir == Dir.BACKWARD ? cur.lastChild() : cur.firstChild();
  while (child) {
    var ret = AutomationUtil.findNodePost(child, dir, pred);
    if (ret)
      return ret;
    child = dir == Dir.BACKWARD ?
        child.previousSibling() : child.nextSibling();
  }

  if (pred(cur))
    return cur;
};

/**
 * Find the next node in the given direction that is either an immediate sibling
 * or a sibling of an ancestor.
 * @param {AutomationNode} cur Node to start search from.
 * @param {Dir} dir
 * @return {AutomationNode}
 */
AutomationUtil.findNextSubtree = function(cur, dir) {
  while (cur) {
    var next = dir == Dir.BACKWARD ?
        cur.previousSibling() : cur.nextSibling();
    if (next)
      return next;
    cur = cur.parent();
  }
};

/**
 * Find the next node in the given direction in depth first order.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {Dir} dir
 * @param {AutomationPredicate.Unary} pred A predicate to apply
 *     to a candidate node.
 * @return {AutomationNode}
 */
AutomationUtil.findNextNode = function(cur, dir, pred) {
  var next = cur;
  do {
    if (!(next = AutomationUtil.findNextSubtree(cur, dir)))
      return null;
    cur = next;
    next = AutomationUtil.findNodePre(next, dir, pred);
  } while (!next);
  return next;
};

/**
 * Given nodes a_1, ..., a_n starting at |cur| in pre order traversal, apply
 * |pred| to a_i and a_(i - 1) until |pred| is satisfied.  Returns a_(i - 1) or
 * a_i (depending on opt_options.before) or null if no match was found.
 * @param {AutomationNode} cur
 * @param {Dir} dir
 * @param {AutomationPredicate.Binary} pred
 * @param {{filter: (AutomationPredicate.Unary|undefined),
 *      before: boolean?}=} opt_options
 *     filter - Filters which candidate nodes to consider. Defaults to leaf
 *         only.
 *     before - True to return a_(i - 1); a_i otherwise. Defaults to false.
 * @return {AutomationNode}
 */
AutomationUtil.findNodeUntil = function(cur, dir, pred, opt_options) {
  opt_options =
      opt_options || {filter: AutomationPredicate.leaf, before: false};
  if (!opt_options.filter)
    opt_options.filter = AutomationPredicate.leaf;

  var before = null;
  var after = null;
  var prev = cur;
  AutomationUtil.findNextNode(cur,
      dir,
      function(candidate) {
        if (!opt_options.filter(candidate))
          return false;

        var satisfied = pred(prev, candidate);

        prev = candidate;
        if (!satisfied)
          before = candidate;
        else
          after = candidate;
        return satisfied;
    });
  return opt_options.before ? before : after;
};

});  // goog.scope
