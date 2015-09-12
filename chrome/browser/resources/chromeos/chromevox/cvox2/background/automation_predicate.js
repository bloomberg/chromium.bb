// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox predicates for the automation extension API.
 */

goog.provide('AutomationPredicate');
goog.provide('AutomationPredicate.Binary');
goog.provide('AutomationPredicate.Unary');

goog.scope(function() {
var RoleType = chrome.automation.RoleType;

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
 * @param {RoleType} role
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.withRole = function(role) {
  return function(node) {
    return node.role == role;
  };
};

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.checkBox = AutomationPredicate.withRole(RoleType.checkBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.comboBox = AutomationPredicate.withRole(RoleType.comboBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.editText = AutomationPredicate.withRole(RoleType.textField);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading = AutomationPredicate.withRole(RoleType.heading);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.withRole(RoleType.inlineTextBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.withRole(RoleType.link);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table = AutomationPredicate.withRole(RoleType.table);

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.button = function(node) {
  return /button/i.test(node.role);
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.formField = function(node) {
  switch (node.role) {
    case 'button':
    case 'buttonDropDown':
    case 'checkBox':
    case 'comboBox':
    case 'date':
    case 'dateTime':
    case 'details':
    case 'disclosureTriangle':
    case 'form':
    case 'menuButton':
    case 'menuListPopup':
    case 'popUpButton':
    case 'radioButton':
    case 'searchBox':
    case 'slider':
    case 'spinButton':
    case 'switch':
    case 'tab':
    case 'textField':
    case 'time':
    case 'toggleButton':
    case 'tree':
      return true;
  }
  return false;
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.landmark = function(node) {
  switch (node.role) {
    case 'application':
    case 'banner':
    case 'complementary':
    case 'contentInfo':
    case 'form':
    case 'main':
    case 'navigation':
    case 'search':
      return true;
  }
  return false;
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.visitedLink = function(node) {
  return node.state.visited;
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.focused = function(node) {
  return node.state.focused;
};

/**
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leaf = function(node) {
  return !node.firstChild ||
      node.role == RoleType.button ||
      node.role == RoleType.buttonDropDown ||
      node.role == RoleType.popUpButton ||
      node.role == RoleType.slider ||
      node.role == RoleType.textField ||
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
      !!(node.name || node.value);
};

/**
 * Matches against non-inline textbox 'nodes' which have an equivalent in the
 *     DOM.
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafDomNode = function(node) {
  return AutomationPredicate.leaf(node) ||
      node.role == RoleType.staticText;
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

/**
 * Leaf nodes that should be ignored while traversing the automation tree. For
 * example, apply this predicate when moving to the next element.
 * @param {chrome.automation.AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.shouldIgnoreLeaf = function(node) {
  if (node.name || node.value)
    return false;

  return AutomationPredicate.leaf(node) &&
      (node.role == RoleType.client ||
       node.role == RoleType.div ||
       node.role == RoleType.group ||
       node.role == RoleType.image ||
       node.role == RoleType.staticText);
};

});  // goog.scope
