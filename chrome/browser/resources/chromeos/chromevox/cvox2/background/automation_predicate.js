// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox predicates for the automation extension API.
 */

goog.provide('AutomationPredicate');
goog.provide('AutomationPredicate.Binary');
goog.provide('AutomationPredicate.Unary');

/**
 * @constructor
 */
AutomationPredicate = function() {};

/**
 * @typedef {function(chrome.automation.AutomationNode) : boolean}
 */
AutomationPredicate.Unary;

/**
 * @typedef {function(chrome.automation.AutomationNode,
 *                    chrome.automation.AutomationNode) : boolean}
 */
AutomationPredicate.Binary;

/**
 * Constructs a predicate given a role.
 * @param {chrome.automation.RoleType} role
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.makeRolePredicate = function(role) {
  return function(node) {
    return node.role == role;
  };
};

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading =
    AutomationPredicate.makeRolePredicate(
        chrome.automation.RoleType.heading);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.makeRolePredicate(
        chrome.automation.RoleType.inlineTextBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link =
    AutomationPredicate.makeRolePredicate(
        chrome.automation.RoleType.link);

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leaf = function(node) {
  return !node.firstChild ||
      node.role == chrome.automation.RoleType.button ||
      node.children.every(function(n) {
        return n.state.invisible;
      });
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafWithText = function(node) {
  return AutomationPredicate.leaf(node) &&
      !!(node.attributes.name || node.attributes.value);
};

/**
 * @param {chrome.automation.AutomationNode} first
 * @param {chrome.automation.AutomationNode} second
 * @return {boolean}
 */
AutomationPredicate.linebreak = function(first, second) {
  // TODO(dtseng): Use next/previousOnLin once available.
  var fl = first.location;
  var sl = second.location;
  return fl.top != sl.top ||
      (fl.top + fl.height != sl.top + sl.height);
};
