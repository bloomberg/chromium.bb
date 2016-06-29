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
var AutomationNode = chrome.automation.AutomationNode;
var RoleType = chrome.automation.RoleType;

/**
 * @constructor
 */
AutomationPredicate = function() {};

/**
 * @typedef {function(!AutomationNode) : boolean}
 */
AutomationPredicate.Unary;

/**
 * @typedef {function(!AutomationNode, !AutomationNode) : boolean}
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
AutomationPredicate.heading = AutomationPredicate.withRole(RoleType.heading);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.withRole(RoleType.inlineTextBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.withRole(RoleType.link);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table = AutomationPredicate.withRole(RoleType.table);

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.button = function(node) {
  return /button/i.test(node.role);
};


/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.editText = function(node) {
  return node.state.editable &&
      node.parent &&
      !node.parent.state.editable;
};

/**
 * @param {!AutomationNode} node
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
 * @param {!AutomationNode} node
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
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.visitedLink = function(node) {
  return node.state.visited;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.focused = function(node) {
  return node.state.focused;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leaf = function(node) {
  return !node.firstChild ||
      node.role == RoleType.button ||
      node.role == RoleType.buttonDropDown ||
      node.role == RoleType.popUpButton ||
      node.role == RoleType.slider ||
      node.role == RoleType.textField ||
      node.state.invisible ||
      node.children.every(function(n) {
        return n.state.invisible;
      });
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafWithText = function(node) {
  return AutomationPredicate.leaf(node) &&
      !!(node.name || node.value);
};

/**
 * Non-inline textbox nodes which have an equivalent in the DOM.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafDomNode = function(node) {
  return AutomationPredicate.leaf(node) ||
      node.role == RoleType.staticText;
};

/**
 * Matches against nodes visited during object navigation. An object as
 * defined below, are all nodes that are focusable or static text. When used in
 * tree walking, it should visit all nodes that tab traversal would as well as
 * non-focusable static text.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.object = function(node) {
  return node.state.focusable ||
      (AutomationPredicate.leafDomNode(node) &&
       (/\S+/.test(node.name) ||
        (node.role != RoleType.lineBreak &&
         node.role != RoleType.staticText &&
         node.role != RoleType.inlineTextBox)));
};

/**
 * @param {!AutomationNode} first
 * @param {!AutomationNode} second
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
 * Matches against a node that contains other interesting nodes.
 * These nodes should always have their subtrees scanned when navigating.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.container = function(node) {
  return AutomationPredicate.structuralContainer(node) ||
      node.role == RoleType.div ||
      node.role == RoleType.document ||
      node.role == RoleType.group ||
      node.role == RoleType.listItem ||
      node.role == RoleType.toolbar ||
      node.role == RoleType.window;
};

/**
 * Matches against nodes that contain interesting nodes, but should never be
 * visited.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.structuralContainer = function(node) {
  return node.role == RoleType.rootWebArea ||
      node.role == RoleType.embeddedObject ||
      node.role == RoleType.iframe ||
      node.role == RoleType.iframePresentational;
};

/**
 * Returns whether the given node should not be crossed when performing
 * traversals up the ancestry chain.
 * @param {AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.root = function(node) {
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
 * Nodes that should be ignored while traversing the automation tree. For
 * example, apply this predicate when moving to the next object.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.shouldIgnoreNode = function(node) {
  // Ignore invisible nodes.
  if (node.state.invisible ||
      (node.location.height == 0 && node.location.width == 0))
    return true;

  // Ignore structural containres.
  if (AutomationPredicate.structuralContainer(node))
    return true;

  // Ignore list markers since we already announce listitem role.
  if (node.role == RoleType.listMarker)
    return true;

  // Don't ignore nodes with names.
  if (node.name || node.value || node.description)
    return false;

  // Ignore some roles.
  return AutomationPredicate.leaf(node) &&
      (node.role == RoleType.client ||
       node.role == RoleType.div ||
       node.role == RoleType.group ||
       node.role == RoleType.image ||
       node.role == RoleType.staticText);
};


/**
 * Returns if the node has a meaningful checked state.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.checkable = function(node) {
  return node.role == RoleType.checkBox ||
      node.role == RoleType.radioButton ||
      node.role == RoleType.menuItemCheckBox ||
      node.role == RoleType.menuItemRadio;
};

});  // goog.scope
