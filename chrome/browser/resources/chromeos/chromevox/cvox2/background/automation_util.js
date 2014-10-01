// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox utilities for the automation extension API.
 */

goog.provide('cvox2.AutomationPredicates');
goog.provide('cvox2.AutomationUtil');
goog.provide('cvox2.Dir');

/**
 * @constructor
 */
cvox2.AutomationPredicates = function() {};

/**
 * Constructs a predicate given a role.
 * @param {string} role
 * @return {function(AutomationNode) : boolean}
 */
cvox2.AutomationPredicates.makeRolePredicate = function(role) {
  return function(node) {
    return node.role == role;
  };
};

/** @type {function(AutomationNode) : boolean} */
cvox2.AutomationPredicates.heading =
    cvox2.AutomationPredicates.makeRolePredicate(
        chrome.automation.RoleType.heading);
/** @type {function(AutomationNode) : boolean} */
cvox2.AutomationPredicates.inlineTextBox =
    cvox2.AutomationPredicates.makeRolePredicate(
        chrome.automation.RoleType.inlineTextBox);
/** @type {function(AutomationNode) : boolean} */
cvox2.AutomationPredicates.link =
    cvox2.AutomationPredicates.makeRolePredicate(
        chrome.automation.RoleType.link);

/**
 * Possible directions to perform tree traversals.
 * @enum {string}
 */
cvox2.Dir = {
  // Search from left to right.
  FORWARD: 'forward',

  // Search from right to left.
  BACKWARD: 'backward'
};

/**
 * @constructor
 */
cvox2.AutomationUtil = function() {};

/**
 * Find a node in subtree of |cur| satisfying |pred| using pre-order traversal.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {cvox2.Dir} dir
 * @param {function(AutomationNode) : boolean} pred A predicate to apply to a
 *     candidate node.
 * @return {AutomationNode}
 */
cvox2.AutomationUtil.findNodePre = function(cur, dir, pred) {
  if (pred(cur))
    return cur;

  var child = dir == cvox2.Dir.BACKWARD ? cur.lastChild() : cur.firstChild();
  while (child) {
    var ret = cvox2.AutomationUtil.findNodePre(child, dir, pred);
    if (ret)
      return ret;
    child = dir == cvox2.Dir.BACKWARD ?
        child.previousSibling() : child.nextSibling();
  }
};

/**
 * Find a node in subtree of |cur| satisfying |pred| using post-order traversal.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {cvox2.Dir} dir
 * @param {function(AutomationNode) : boolean} pred A predicate to apply to a
 *     candidate node.
 * @return {AutomationNode}
 */
cvox2.AutomationUtil.findNodePost = function(cur, dir, pred) {
  var child = dir == cvox2.Dir.BACKWARD ? cur.lastChild() : cur.firstChild();
  while (child) {
    var ret = cvox2.AutomationUtil.findNodePost(child, dir, pred);
    if (ret)
      return ret;
    child = dir == cvox2.Dir.BACKWARD ?
        child.previousSibling() : child.nextSibling();
  }

  if (pred(cur))
    return cur;
};

/**
 * Find the next node in the given direction that is either an immediate
 * sibling or a sibling of an ancestor.
 * @param {AutomationNode} cur Node to start search from.
 * @param {cvox2.Dir} dir
 * @return {AutomationNode}
 */
cvox2.AutomationUtil.findNextSubtree = function(cur, dir) {
  while (cur) {
    var next = dir == cvox2.Dir.BACKWARD ?
        cur.previousSibling() : cur.nextSibling();
    if (next)
      return next;

    cur = cur.parent();
  }
};

/**
 * Find the next node in the given direction in depth first order.
 * @param {AutomationNode} cur Node to begin the search from.
 * @param {cvox2.Dir} dir
 * @param {function(AutomationNode) : boolean} pred A predicate to apply to a
 *     candidate node.
 * @return {AutomationNode}
 */
cvox2.AutomationUtil.findNextNode = function(cur, dir, pred) {
  var next = cur;
  do {
    if (!(next = cvox2.AutomationUtil.findNextSubtree(cur, dir)))
      return null;
    cur = next;
    next = cvox2.AutomationUtil.findNodePre(next, dir, pred);
  } while (!next);
  return next;
};
